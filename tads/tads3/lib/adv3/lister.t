#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - Lister class
 *   
 *   This module defines the "Lister" class, which generates formatted
 *   lists of objects, and several subclasses of Lister that generate
 *   special kinds of lists.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Lister.  This is the base class for formatting of lists of objects.
 *   
 *   The external interface consists of the showList() method, which
 *   displays a formatted list of objects according to the rules of the
 *   lister subclass.
 *   
 *   The rest of the methods are an internal interface which lister
 *   subclasses can override to customize the way that a list is shown.
 *   Certain of these methods are meant to be overridden by virtually all
 *   listers, such as the methods that show the prefix and suffix
 *   messages.  The remaining methods are designed to allow subclasses to
 *   customize detailed aspects of the formatting, so they only need to be
 *   overridden when something other than the default behavior is needed.  
 */
class Lister: object
    /*
     *   Show a list, showing all items in the list as though they were
     *   fully visible, regardless of their actual sense status.  
     */
    showListAll(lst, options, indent)
    {
        /* create a sense information table with each item in full view */
        local infoTab = new LookupTable(16, 32);
        foreach (local cur in lst)
        {
            /* add a plain view sensory description to the info list */
            infoTab[cur] = new SenseInfo(cur, transparent, nil, 3);
        }
        
        /* show the list from the current global point of view */
        showList(getPOV(), nil, lst, options, indent, infoTab, nil);
    }

    /*
     *   Display a list of items, grouping according to the 'listWith'
     *   associations of the items.  We will only list items for which
     *   isListed() returns true.
     *   
     *   'pov' is the point of view of the listing, which is usually an
     *   actor (and usually the player character actor).
     *   
     *   'parent' is the parent (container) of the list being shown.  This
     *   should be nil if the listed objects are not all within a single
     *   object.
     *   
     *   'lst' is the list of items to display.
     *   
     *   'options' gives a set of ListXxx option flags.
     *   
     *   'indent' gives the indentation level.  This is used only for
     *   "tall" lists (specified by including ListTall in the options
     *   flags).  An indentation level of zero indicates no indentation.
     *   
     *   'infoTab' is a lookup table of SenseInfo objects for all of the
     *   objects that can be sensed from the perspective of the actor
     *   performing the action that's causing the listing.  This is
     *   normally the table returned from Thing.senseInfoTable() for the
     *   actor from whose point of view the list is being generated.  (We
     *   take this as a parameter rather than generating ourselves for two
     *   reasons.  First, it's often the case that the same information
     *   table will be needed for a series of listings, so we can save the
     *   compute time of recalculating the same table repeatedly by having
     *   the caller obtain the table and pass it to each lister.  Second,
     *   in some cases the caller will want to synthesize a special sense
     *   table rather than using the actual sense information; taking this
     *   as a parameter allows the caller to easily customize the table.)
     *   
     *   'parentGroup' is the ListGroup object that is showing this list.
     *   We will not group the objects we list into the parent group, or
     *   into any group more general than the parent group.  
     *   
     *   This routine is not usually overridden in lister subclasses.
     *   Instead, this method calls a number of other methods that
     *   determine the listing style in more detail; usually those other,
     *   simpler methods are customized in subclasses.  
     */
    showList(pov, parent, lst, options, indent, infoTab, parentGroup)
    {
        /* remember the original list */
        local origLst = lst;

        /* filter the list to get only the items we actually will list */
        lst = getFilteredList(lst, infoTab);

        /* create a lookup table to keep track of the groups we've seen */
        local groupTab = new LookupTable();
        local groups = new Vector(10);
        
        /* set up a vector to keep track of the singles */
        local singles = new Vector(10);

        /* figure the groupings */
        local itemCount = getListGrouping(groupTab, groups, singles,
                                          lst, parentGroup);

        /*
         *   Now that we've figured out what's in the list and how it's
         *   arranged into groups, show the list.  
         */
        showArrangedList(pov, parent, lst, options, indent, infoTab,
                         itemCount, singles, groups, groupTab, origLst);

        /* 
         *   If the list is recursive, mention the contents of any items
         *   that weren't listed in the main list, and of any contents
         *   that are specifically to be listed out-of-line.  Don't do
         *   this if we're already recursively showing such a listing,
         *   since if we did so we could show items at recursive depths
         *   more than once; if we're already doing a recursive listing,
         *   our caller will itself recurse through all levels of the
         *   tree, so we don't have to recurse any further ourselves.  
         */
        if ((options & ListRecurse) != 0
            && indent == 0
            && (options & ListContents) == 0)
        {
            /* show the contents of each object we didn't list */
            showSeparateContents(pov, origLst,
                                 options | ListContents, infoTab);
        }
    }

    /*
     *   Filter a list to get only the elements we actually want to show.
     *   Returns a new list consisting only of the items that (1) pass the
     *   isListed() test, and (2) are represented in the sense information
     *   table (infoTab).  If infoTab is nil, no sense filtering is
     *   applied.  
     */
    getFilteredList(lst, infoTab)
    {
        /* narrow the list down based on the isListed criteria */
        lst = lst.subset({x: isListed(x)});
        
        /* 
         *   If we have an infoTab, build a new list consisting only of
         *   the items in 'lst' that have infoTab entries - we can't sense
         *   anything that doesn't have an infoTab entry, so we don't want
         *   to show any such objects.  
         */
        if (infoTab != nil)
        {
            /* create a vector to contain the new filtered list */
            local filteredList = new Vector(lst.length());
            
            /* 
             *   run through our original list and confirm that each one
             *   is in the infoTab
             */
            foreach (local cur in lst)
            {
                /* 
                 *   if this item has an infoTab entry, add this item to
                 *   the filtered list 
                 */
                if (infoTab[cur] != nil)
                    filteredList.append(cur);
            }
            
            /* forget the original list, and use the filtered list instead */
            lst = filteredList;
        }

        /* return the filtered list */
        return lst;
    }
    
    /*
     *   Get the groupings for a given listing.
     *   
     *   'groupTab' is an empty LookupTable, and 'groups' is an empty
     *   Vector; we'll populate these with the grouping information.
     *   'singles' is an empty Vector that we'll populate with the single
     *   items not part of any group.  
     */
    getListGrouping(groupTab, groups, singles, lst, parentGroup)
    {
        local cur;
        local i, cnt;

        /* 
         *   First, scan the list to determine how we're going to group
         *   the objects.
         */
        for (i = 1, cnt = lst.length() ; i <= cnt ; ++i)
        {
            local curGroups;
            local parentIdx;
            
            /* get this object into a local for easy reference */
            cur = lst[i];
            
            /* if the item isn't part of this listing, skip it */
            if (!isListed(cur))
                continue;

            /* get the list of groups with which this object is listed */
            curGroups = listWith(cur);

            /* if there are no groups, we can move on to the next item */
            if (curGroups == nil)
                continue;

            /* 
             *   If we have a parent group, and it appears in the list of
             *   groups for this item, eliminate everything in the item's
             *   group list up to and including the parent group.  If
             *   we're showing this list as part of a group to begin with,
             *   we obviously don't want to show this list grouped into
             *   the same group, and we also don't want to group it into
             *   anything broader than the parent group.  Groups are
             *   listed from most general to most specific, so we can
             *   eliminate anything up to and including the parent group. 
             */
            if (parentGroup != nil
                && (parentIdx = curGroups.indexOf(parentGroup)) != nil)
            {
                /* eliminate everything up to and including the parent */
                curGroups = curGroups.sublist(parentIdx + 1);
            }

            /* if this item has no groups, skip it */
            if (curGroups.length() == 0)
                continue;

            /*
             *   This item has one or more group associations that we must
             *   consider.
             */
            foreach (local g in curGroups)
            {
                local itemsInGroup;
                
                /* find the group table entry for this group */
                itemsInGroup = groupTab[g];

                /* if there's no entry for this group, create a new one */
                if (itemsInGroup == nil)
                {
                    /* create a new group table entry */
                    itemsInGroup = groupTab[g] = new Vector(10);

                    /* add it to the group vector */
                    groups.append(g);
                }

                /* 
                 *   add this item to the list of items that want to be
                 *   grouped with this group 
                 */
                itemsInGroup.append(cur);
            }
        }

        /*
         *   We now have the set of all of the groups that could possibly
         *   be involved in this list display.  We must now choose the
         *   single group we'll use to display each grouped object.
         *   
         *   First, eliminate any groups with insufficient membership.
         *   (Most groups require at least two members, but this can vary
         *   by group.)  
         */
        for (i = 1, cnt = groups.length() ; i <= cnt ; ++i)
        {
            /* if this group has only one member, drop it */
            if (groupTab[groups[i]].length() < groups[i].minGroupSize)
            {
                /* remove this group from the group list */
                groups.removeElementAt(i);

                /* 
                 *   adjust the list count, and back up to try the element
                 *   newly at this index on the next iteration 
                 */
                --cnt;
                --i;
            }
        }

        /*
         *   Next, scan for groups with identical member lists, and for
         *   groups with subset member lists.  For each pair of identical
         *   elements we find, eliminate the more general of the two.  
         */
        for (i = 1, cnt = groups.length() ; i <= cnt ; ++i)
        {
            local g1;
            local mem1;

            /* get the current group and its membership list */
            g1 = groups[i];
            mem1 = groupTab[g1];

            /* look for matching items in the list after this one */
            for (local j = i + 1 ; j <= cnt ; ++j)
            {
                local g2;
                local mem2;

                /* get the current item and its membership list */
                g2 = groups[j];
                mem2 = groupTab[g2];
                
                /*
                 *   Compare the membership lists for the two items.  Note
                 *   that we built these membership lists all in the same
                 *   order of objects, so if two membership lists have all
                 *   the same members, those members will be in the same
                 *   order in the two lists; hence, we can simply compare
                 *   the two lists to determine the membership order.  
                 */
                if (mem1 == mem2)
                {
                    local ordList;
                    
                    /* 
                     *   The groups have identical membership, so
                     *   eliminate the more general group.  Groups are
                     *   ordered from most general to least general, so
                     *   keep the one with the higher index in the group
                     *   list for an object in the membership list.  Note
                     *   that we assume that each member has the same
                     *   ordering for the common groups, so we can pick a
                     *   member arbitrarily to find the way a member
                     *   orders the groups.  
                     */
                    ordList = listWith(mem1[1]);
                    if (ordList.indexOf(g1) > ordList.indexOf(g2))
                    {
                        /* 
                         *   group g1 is more specific than group g2, so
                         *   keep g1 and discard g2 - remove the 'j'
                         *   element from the list, and back up in the
                         *   inner loop so we reconsider the element newly
                         *   at this index on the next iteration 
                         */
                        groups.removeElementAt(j);
                        --cnt;
                        --j;
                    }
                    else
                    {
                        /* 
                         *   group g2 is more specific, so discard g1 -
                         *   remove the 'i' element from the list, back up
                         *   in the outer loop, and break out of the inner
                         *   loop, since the outer loop element is no
                         *   longer there for us to consider in comparing
                         *   more elements in the inner loop 
                         */
                        groups.removeElementAt(i);
                        --cnt;
                        --i;
                        break;
                    }
                }
            }
        }

        /*
         *   Scan for subsets.  For each group whose membership list is a
         *   subset of another group in our list, eliminate the subset,
         *   keeping only the larger group.  The group lister will be able
         *   to show the subgroup as grouped within its larger list.  
         */
        for (local i = 1, cnt = groups.length() ; i <= cnt ; ++i)
        {
            local g1;
            local mem1;

            /* get the current group and its membership list */
            g1 = groups[i];
            mem1 = groupTab[g1];

            /* look at the other elements to see if we have any subsets */
            for (local j = 1 ; j <= cnt ; ++j)
            {
                local g2;
                local mem2;

                /* don't bother checking the same element */
                if (j == i)
                    continue;

                /* get the current item and its membership list */
                g2 = groups[j];
                mem2 = groupTab[g2];

                /* 
                 *   if g2's membership is a subset, eliminate g2 from the
                 *   group list 
                 */
                if (isListSubset(mem2, mem1))
                {
                    /* remove g2 from the list */
                    groups.removeElementAt(j);

                    /* adjust the loop counters for the removal */
                    --cnt;
                    --j;

                    /* 
                     *   adjust the outer loop counter if it's affected -
                     *   the outer loop is affected if it's already past
                     *   this point in the list, which means that its
                     *   index is higher than the inner loop index 
                     */
                    if (i > j)
                        --i;
                }
            }
        }

        /*
         *   We now have a final accounting of the groups that we will
         *   consider using.  Reset the membership list for each group in
         *   the surviving list. 
         */
        foreach (local g in groups)
        {
            local itemsInList;

            /* get this group's membership list vector */
            itemsInList = groupTab[g];

            /* clear the vector */
            itemsInList.removeRange(1, itemsInList.length());
        }

        /*
         *   Now, run through our item list again, and assign each item to
         *   the surviving group that comes earliest in the item's group
         *   list.  
         */
        for (i = 1, cnt = lst.length() ; i <= cnt ; ++i)
        {
            local curGroups;
            local winningGroup;

            /* get this object into a local for easy reference */
            cur = lst[i];
            
            /* if the item isn't part of this listing, skip it */
            if (!isListed(cur))
                continue;

            /* get the list of groups with which this object is listed */
            curGroups = listWith(cur);
            if (curGroups == nil)
                curGroups = [];

            /* 
             *   find the first element in the group list that is in the
             *   surviving group list
             */
            winningGroup = nil;
            foreach (local g in curGroups)
            {
                /* if this group is in the surviving list, it's the one */
                if (groups.indexOf(g) != nil)
                {
                    winningGroup = g;
                    break;
                }
            }

            /* 
             *   if we have a group, add this item to the group's
             *   membership; otherwise, add it to the singles list 
             */
            if (winningGroup != nil)
                groupTab[winningGroup].append(cur);
            else
                singles.append(cur);
        }

        /* eliminate any surviving group with too few members */
        for (i = 1, cnt = groups.length() ; i <= cnt ; ++i)
        {
            local mem;

            /* get this group's membership list */
            mem = groupTab[groups[i]];

            /* 
             *   if this group's membership is too small, eliminate the
             *   group and add the member into the singles pile 
             */
            if (mem.length() < groups[i].minGroupSize)
            {
                /* put the item(s) into the singles list */
                singles.appendAll(mem);

                /* eliminate this item from the group list */
                groups.removeElementAt(i);

                /* adjust the loop counters */
                --cnt;
                --i;
            }
        }

        /* return the cardinality of the arranged list */
        return getArrangedListCardinality(singles, groups, groupTab);
    }

    /*
     *   Show the list.  This is called after we've figured out which items
     *   we intend to display, and after we've arranged the items into
     *   groups.  In rare cases, listers might want to override this, to
     *   customize the way the way the list is displayed based on the
     *   internal arrangement of the list.  
     */
    showArrangedList(pov, parent, lst, options, indent, infoTab, itemCount,
                     singles, groups, groupTab, origLst)
    {
        /*
         *   We now know how many items we're listing (grammatically
         *   speaking), so we're ready to display the list prefix.  If
         *   we're displaying nothing at all, just display the "empty"
         *   message, and we're done.  
         */
        if (itemCount == 0)
        {
            /* show the empty list */
            showListEmpty(pov, parent);
        }
        else
        {
            local i;
            local cnt;
            local sublists;
            local origOptions = options;
            local itemOptions;
            local groupOptions;
            local listCount;
            local dispCount;
            local cur;

            /* 
             *   Check to see if we have one or more group sublists - if
             *   we do, we must use the "long" list format for our overall
             *   list, otherwise we can use the normal "short" list
             *   format.  The long list format uses semicolons to separate
             *   items.  
             */
            for (i = 1, cnt = groups.length(), sublists = nil ;
                 i <= cnt ; ++i)
            {
                /* 
                 *   if this group's lister item displays a sublist, we
                 *   must use the long format 
                 */
                if (groups[i].groupDisplaysSublist)
                {
                    /* note that we are using the long format */
                    sublists = true;
                    
                    /* 
                     *   one is enough to make us use the long format, so
                     *   we need not look any further 
                     */
                    break;
                }
            }
            
            /* generate the prefix message if we're in a 'tall' listing */
            if ((options & ListTall) != 0)
            {
                /* indent the prefix */
                showListIndent(options, indent);
                
                /* 
                 *   Show the prefix.  If this is a contents listing, and
                 *   it's not at the top level, show the contents prefix;
                 *   otherwise show the full list prefix.  Note that we can
                 *   have a contents listing at the top level, since some
                 *   lists are broken out for separate listing.  
                 */
                if ((options & ListContents) != 0 && indent != 0)
                    showListContentsPrefixTall(itemCount, pov, parent);
                else
                    showListPrefixTall(itemCount, pov, parent);
                
                /* go to a new line for the list contents */
                "\n";
                
                /* indent the items one level now, since we showed a prefix */
                ++indent;
            }
            else
            {
                /* show the prefix */
                showListPrefixWide(itemCount, pov, parent, lst: lst);
            }
            
            /* 
             *   regardless of whether we're adding long formatting to the
             *   main list, display the group sublists with whatever
             *   formatting we were originally using 
             */
            groupOptions = options;
            
            /* show each item with our current set of options */
            itemOptions = options;
            
            /* 
             *   if we're using sublists, show "long list" separators in
             *   the main list 
             */
            if (sublists)
                itemOptions |= ListLong;
            
            /* 
             *   calculate the number of items we'll show in the list -
             *   each group shows up as one list entry, so the total
             *   number of list entries is the number of single items plus
             *   the number of groups 
             */
            listCount = singles.length() + groups.length();

            /*
             *   Show the items.  Run through the (filtered) original
             *   list, so that we show everything in the original sorting
             *   order.  
             */
            dispCount = 0;
            foreach (cur in lst)
            {
                local group;
                local displayedCur;
                
                /* presume we'll display this item */
                displayedCur = true;
                
                /*
                 *   Figure out how to show this item: if it's in the
                 *   singles list, show it as a single item; if it's in
                 *   the group list, show its group; if it's in a group
                 *   we've previously shown, show nothing, as we showed
                 *   the item when we showed its group.  
                 */
                if (singles.indexOf(cur) != nil)
                {
                    /*
                     *   It's in the singles list, so show it as a single
                     *   item.
                     *   
                     *   If the item has contents that we'll display in
                     *   'tall' mode, show the item with its contents - we
                     *   don't need to show the item separately, since it
                     *   will provide a 'tall' list prefix showing itself.
                     *   Otherwise, show the item singly.  
                     */
                    if ((options & ListTall) != 0
                        && (options & ListRecurse) != 0
                        && contentsListed(cur)
                        && !contentsListedSeparately(cur)
                        && getListedContents(cur, infoTab) != [])
                    {
                        /* show the item with its contents */
                        showContentsList(pov, cur, origOptions | ListContents,
                                         indent, infoTab);
                    }
                    else
                    {
                        /* show the list indent if necessary */
                        showListIndent(itemOptions, indent);
                        
                        /* show the item */
                        showListItem(cur, itemOptions, pov, infoTab);
                        
                        /* 
                         *   if we're in wide recursive mode, show the
                         *   item's contents as an in-line parenthetical 
                         */
                        if ((options & ListTall) == 0
                            && (options & ListRecurse) != 0
                            && contentsListed(cur)
                            && !contentsListedSeparately(cur))
                        {
                            /* show the item's in-line contents */
                            showInlineContentsList(pov, cur,
                                origOptions | ListContents,
                                indent + 1, infoTab);
                        }
                    }
                }
                else if ((group = groups.valWhich(
                    {g: groupTab[g].indexOf(cur) != nil})) != nil)
                {
                    /* show the list indent if necessary */
                    showListIndent(itemOptions, indent);
                    
                    /* we found the item in a group, so show its group */
                    group.showGroupList(pov, self, groupTab[group],
                                        groupOptions, indent, infoTab);

                    /* 
                     *   Forget this group - we only need to show each
                     *   group once, since the group shows every item it
                     *   contains.  Since we'll encounter the groups other
                     *   members as we continue to scan the main list, we
                     *   want to make sure we don't show the group again
                     *   when we reach the other items.  
                     */
                    groups.removeElement(group);
                }
                else
                {
                    /* 
                     *   We didn't find the item in the singles list or in
                     *   a group - it must be part of a group that we
                     *   already showed previously, so we don't need to
                     *   show it again now.  Simply make a note that we
                     *   didn't display it.  
                     */
                    displayedCur = nil;
                }
                
                /* if we displayed the item, show a suitable separator */
                if (displayedCur)
                {
                    /* count another list entry displayed */
                    ++dispCount;
                    
                    /* show an appropriate separator */
                    showListSeparator(itemOptions, dispCount, listCount);
                }
            }

            /* 
             *   if we're in 'wide' mode, finish the listing (note that if
             *   this is a 'tall' listing, we're already done, because a
             *   tall listing format doesn't make provisions for anything
             *   after the item list) 
             */
            if ((options & ListTall) == 0)
            {
                /* show the wide-mode list suffix */
                showListSuffixWide(itemCount, pov, parent);
            }
        }
    }

    /*
     *   Get the cardinality of an arranged list.  Returns the number of
     *   items that will appear in the list, for grammatical agreement.  
     */
    getArrangedListCardinality(singles, groups, groupTab)
    {
        local cnt;

        /* start with a count of zero; we'll add to it as we go */
        cnt = 0;
        
        /* 
         *   Add up the cardinality of the single items.  Some individual
         *   items in the singles list might count as multiple items
         *   grammatically - in particular, if an item has a plural name,
         *   we need a plural verb to agree with it. 
         */
        foreach (local s in singles)
        {
            /* add the grammatical cardinality of this single item */
            cnt += listCardinality(s);
        }
        
        /* add in the cardinality of each group */
        foreach (local g in groups)
        {
            /* add the grammatical cardinality of this group */
            cnt += g.groupCardinality(self, groupTab[g]);
        }

        /* return the total */
        return cnt;
    }

    /*
     *   Get the number of noun phrase elements in a list.  This differs
     *   from the cardinality in that we only count noun phrases, not the
     *   cardinality of each noun phrase.  So, for example, "five coins"
     *   has cardinality five, but has only one noun phrase.  
     */
    getArrangedListNounPhraseCount(singles, groups, groupTab)
    {
        local cnt;
        
        /* each single item counts as one noun phrase */
        cnt = singles.length();
        
        /* add in the noun phrases from each group */
        foreach (local g in groups)
            cnt += g.groupNounPhraseCount(self, groupTab[g]);

        /* return the total */
        return cnt;
    }

    /*
     *   Service routine: show the separately-listed contents of the items
     *   in the given list, and their separately-listed contents, and so
     *   on.  This routine is not normally overridden in subclasses, and is
     *   not usually called except from the Lister implementation.
     *   
     *   For each item in the given list, we show the item's contents if
     *   the item is either marked as unlisted, or it's marked as showing
     *   its contents separately.  In the former case, we know that we
     *   cannot have shown the item's contents in-line in the main list,
     *   since we didn't show the item at all in the main list.  In the
     *   latter case, we know that we didn't show the item's contents in
     *   the main list because it's specifically marked as showing its
     *   contents out-of-line.  
     */
    showSeparateContents(pov, lst, options, infoTab)
    {
        /* 
         *   show the separate contents list for each item in the list
         *   which isn't itself listable or which has its contents listed
         *   separately despite its being listed 
         */
        foreach (local cur in lst)
        {
            /* only show the contents if the contents are listed */
            if (contentsListed(cur))
            {
                /* 
                 *   if we didn't list this item, or if it specifically
                 *   wants its contents listed out-of-line, show its
                 *   listable contents 
                 */
                if (!isListed(cur) || contentsListedSeparately(cur))
                {
                    /* 
                     *   Show the item's contents.  Note that even though
                     *   we're showing this list recursively, it's actually
                     *   a new top-level list, so show it at indent level
                     *   zero.  
                     */
                    showContentsList(pov, cur, options, 0, infoTab);
                }

                /* recursively do the same thing with its contents */
                showSeparateContents(pov, getContents(cur), options, infoTab);
            }
        }
    }

    /*	
     *   Show a list indent if necessary.  If ListTall is included in the
     *   options, we'll indent to the given level; otherwise we'll do
     *   nothing.  
     */
    showListIndent(options, indent)
    {
        /* show the indent only if we're in "tall" mode */
        if ((options & ListTall) != 0)
        {
            for (local i = 0 ; i < indent ; ++i)
                "\t";
        }
    }

    /*
     *   Show a newline after a list item if we're in a tall list; does
     *   nothing for a wide list.  
     */
    showTallListNewline(options)
    {
        if ((options & ListTall) != 0)
            "\n";
    }

    /*
     *   Show a simple list, recursing into contents lists if necessary.
     *   We pay no attention to grouping; we just show the items
     *   individually.
     *   
     *   'prevCnt' is the number of items already displayed, if anything
     *   has already been displayed for this list.  This should be zero if
     *   this will display the entire list.  
     */
    showListSimple(pov, lst, options, indent, prevCnt, infoTab)
    {
        local i;
        local cnt;
        local dispCount;
        local totalCount;

        /* calculate the total number of items in the lis t*/
        totalCount = prevCnt + lst.length();
        
        /* display the items */
        for (i = 1, cnt = lst.length(), dispCount = prevCnt ; i <= cnt ; ++i)
        {
            local cur;
            
            /* get the item */
            cur = lst[i];
            
            /* 
             *   If the item has contents that we'll display in 'tall'
             *   mode, show the item with its contents - we don't need to
             *   show the item separately, since it will provide a 'tall'
             *   list prefix showing itself.  Otherwise, show the item
             *   singly.  
             */
            if ((options & ListTall) != 0
                && (options & ListRecurse) != 0
                && contentsListed(cur)
                && !contentsListedSeparately(cur)
                && getListedContents(cur, infoTab) != [])
            {
                /* show the item with its contents */
                showContentsList(pov, cur, options | ListContents,
                                 indent + 1, infoTab);
            }
            else
            {
                /* show the list indent if necessary */
                showListIndent(options, indent);
                
                /* show the item */
                showListItem(cur, options, pov, infoTab);

                /* 
                 *   if we're in wide recursive mode, show the item's
                 *   contents as an in-line parenthetical 
                 */
                if ((options & ListTall) == 0
                    && (options & ListRecurse) != 0
                    && contentsListed(cur)
                    && !contentsListedSeparately(cur))
                {
                    /* show the item's in-line contents */
                    showInlineContentsList(pov, cur,
                                           options | ListContents,
                                           indent + 1, infoTab);
                }
            }

            /* count the item displayed */
            ++dispCount;

            /* show the list separator */
            showListSeparator(options, dispCount, totalCount);
        }
    }

    /*
     *   List the contents of an item.
     *   
     *   'pov' is the point of view, which is usually an actor (and
     *   usually the player character actor).
     *   
     *   'obj' is the item whose contents we are to display.
     *   
     *   'options' is the set of flags that we'll pass to showList(), and
     *   has the same meaning as for that function.
     *   
     *   'infoTab' is a lookup table of SenseInfo objects giving the sense
     *   information for all of the objects that the actor to whom we're
     *   showing the contents listing can sense.  
     */
    showContentsList(pov, obj, options, indent, infoTab)
    {
        /* 
         *   List the item's contents.  By default, use the contentsLister
         *   property of the object whose contents we're showing to obtain
         *   the lister for the contents.  
         */
        obj.showObjectContents(pov, obj.contentsLister, options,
                               indent, infoTab);
    }

    /*
     *   Determine if an object's contents are listed separately from its
     *   own list entry for the purposes of our type of listing.  If this
     *   returns true, then we'll list the object's contents in a separate
     *   listing (a separate sentence following the main listing sentence,
     *   or a separate tree when in 'tall' mode).
     *   
     *   Note that this only matters for objects listed in the top-level
     *   list.  We'll always show the contents separately for an object
     *   that isn't listed in the top-level list (i.e., an object for which
     *   isListed(obj) returns nil).  
     */
    contentsListedSeparately(obj) { return obj.contentsListedSeparately; }

    /*
     *   Show an "in-line" contents list.  This shows the item's contents
     *   list as a parenthetical, as part of a recursive listing.  This is
     *   pretty much the same as showContentsList(), but uses the object's
     *   in-line contents lister instead of its regular contents lister.  
     */
    showInlineContentsList(pov, obj, options, indent, infoTab)
    {
        /* show the item's contents using its in-line contents lister */
        obj.showObjectContents(pov, obj.inlineContentsLister,
                               options, indent, infoTab);
    }

    /* 
     *   Show the prefix for a 'wide' listing - this is a message that
     *   appears just before we start listing the objects.  'itemCount' is
     *   the number of items to be listed; the items might be grouped in
     *   the listing, so a list that comes out as "three boxes and two
     *   books" will have an itemCount of 5.  (The purpose of itemCount is
     *   to allow the message to have grammatical agreement in number.)
     *   
     *   'lst' is the entire list, which some languages need for
     *   grammatical agreement.  This is passed as a named argument, so an
     *   overrider can omit it from the parameter list if it's not needed.
     *   
     *   This will never be called with an itemCount of zero, because we
     *   will instead use showListEmpty() to display an empty list.  
     */
    showListPrefixWide(itemCount, pov, parent, lst:) { }

    /* 
     *   show the suffix for a 'wide' listing - this is a message that
     *   appears just after we finish listing the objects 
     */
    showListSuffixWide(itemCount, pov, parent) { }

    /* 
     *   Show the list prefix for a 'tall' listing.  Note that there is no
     *   list suffix for a tall listing, because the format doesn't allow
     *   it. 
     */
    showListPrefixTall(itemCount, pov, parent) { }

    /* 
     *   Show the list prefix for the contents of an object in a 'tall'
     *   listing.  By default, we just show our usual tall list prefix.  
     */
    showListContentsPrefixTall(itemCount, pov, parent)
        { showListPrefixTall(itemCount, pov, parent); }

    /*
     *   Show an empty list.  If the list to be displayed has no items at
     *   all, this is called instead of the prefix/suffix routines.  This
     *   can be left empty if no message is required for an empty list, or
     *   can display the complete message appropriate for an empty list
     *   (such as "You are empty-handed").  
     */
    showListEmpty(pov, parent) { }

    /*
     *   Is this item to be listed in room descriptions?  Returns true if
     *   so, nil if not.  By default, we'll use the object's isListed
     *   method to make this determination.  We virtualize this into the
     *   lister interface to allow for different inclusion rules for the
     *   same object depending on the type of list we're generating.  
     */
    isListed(obj) { return obj.isListed(); }

    /*
     *   Get the grammatical cardinality of this listing item.  This should
     *   return the number of items that this item appears to be
     *   grammatically, for noun-verb agreement purposes.  
     */
    listCardinality(obj) { return obj.listCardinality(self); }

    /*
     *   Are this item's contents listable?  
     */
    contentsListed(obj) { return obj.contentsListed; }

    /*
     *   Get all contents of this item. 
     */
    getContents(obj) { return obj.contents; }

    /*
     *   Get the listed contents of an object.  'infoTab' is the sense
     *   information table for the enclosing listing.  By default, we call
     *   the object's getListedContents() method, but this is virtualized
     *   in the lister interface to allow for listing other hierarchies
     *   besides ordinary contents.  
     */
    getListedContents(obj, infoTab)
    {
        return obj.getListedContents(self, infoTab);
    }

    /*
     *   Get the list of grouping objects for listing the item.  By
     *   default, we return the object's listWith result.  Subclasses can
     *   override this to specify different groupings for the same object
     *   depending on the type of list we're generating.
     *   
     *   The group list returned is in order from most general to most
     *   specific.  For example, if an item is grouped with coins in
     *   general and silver coins in particular, the general coins group
     *   would come first, then the silver coin group, because the silver
     *   coin group is more specific.  
     */
    listWith(obj) { return obj.listWith; }

    /* show an item in a list */
    showListItem(obj, options, pov, infoTab)
    {
        obj.showListItem(options, pov, infoTab);
    }

    /* 
     *   Show a set of equivalent items as a counted item ("three coins").
     *   The listing mechanism itself never calls this directly; instead,
     *   this is provided so that ListGroupEquivalent can ask the lister
     *   how to describe its equivalent sets, so that different listers
     *   can customize the display of equivalent items.
     *   
     *   'lst' is the full list of equivalent items.  By default, we pick
     *   one of these arbitrarily to show, since they're presumably all
     *   the same for the purposes of the list.  
     */
    showListItemCounted(lst, options, pov, infoTab)
    {
        /* 
         *   by defualt, show the counted name for one of the items
         *   (chosen arbitrarily, since they're all the same) 
         */
        lst[1].showListItemCounted(lst, options, pov, infoTab);
    }

    /*
     *   Show a list separator after displaying an item.  curItemNum is
     *   the number of the item just displayed (1 is the first item), and
     *   totalItems is the total number of items that will be displayed in
     *   the list.
     *   
     *   This generic routine is further parameterized by properties for
     *   the individual types of separators.  This default implementation
     *   distinguishes the following separators: the separator between the
     *   two items in a list of exactly two items; the separator between
     *   adjacent items other than the last two in a list of more than two
     *   items; and the separator between the last two elements of a list
     *   of more than two items.
     */
    showListSeparator(options, curItemNum, totalItems)
    {
        local useLong = ((options & ListLong) != 0);
        
        /* if this is a tall list, the separator is simply a newline */
        if ((options & ListTall) != 0)
        {
            "\n";
            return;
        }
        
        /* if that was the last item, there are no separators */
        if (curItemNum == totalItems)
            return;
        
        /* check to see if the next item is the last */
        if (curItemNum + 1 == totalItems)
        {
            /* 
             *   We just displayed the penultimate item in the list, so we
             *   need to use the special last-item separator.  If we're
             *   only displaying two items total, we use an even more
             *   special separator.  
             */
            if (totalItems == 2)
            {
                /* use the two-item separator */
                if (useLong)
                    longListSepTwo;
                else
                    listSepTwo;
            }
            else
            {
                /* use the normal last-item separator */
                if (useLong)
                    longListSepEnd;
                else
                    listSepEnd;
            }
        }
        else
        {
            /* in the middle of the list - display the normal separator */
            if (useLong)
                longListSepMiddle;
            else
                listSepMiddle;
        }
    }

    /* 
     *   Show the specific types of list separators for this list.  By
     *   default, these will use the generic separators defined in the
     *   library messages object (gLibMessages).  For English, these are
     *   commas and semicolons for short and long lists, respectively; the
     *   word "and" for a list with only two items; and a comma or
     *   semicolon and the word "and" between the last two items in a list
     *   with more than two items.  
     */

    /* 
     *   normal and "long list" separator between the two items in a list
     *   with exactly two items 
     */
    listSepTwo { gLibMessages.listSepTwo; }
    longListSepTwo { gLibMessages.longListSepTwo; }

    /* 
     *   normal and long list separator between items in list with more
     *   than two items 
     */
    listSepMiddle { gLibMessages.listSepMiddle; }
    longListSepMiddle { gLibMessages.longListSepMiddle; }

    /* 
     *   normal and long list separator between second-to-last and last
     *   items in a list with more than two items 
     */
    listSepEnd { gLibMessages.listSepEnd; }
    longListSepEnd { gLibMessages.longListSepEnd; }

    /*
     *   Get my "top-level" lister.  For a sub-lister, this will return
     *   the parent lister's top-level lister.  The default lister is a
     *   top-level lister, so we just return ourself.  
     */
    getTopLister() { return self; }

    /*
     *   The last custom flag defined by this class.  Lister and each
     *   subclass are required to define this so that each subclass can
     *   allocate its own custom flags in a manner that adapts
     *   automatically to future additions of flags to base classes.  As
     *   the base class, we allocate our flags statically with #define's,
     *   so we simply use the fixed #define'd last flag value here.
     */
    nextCustomFlag = ListCustomFlag
;


/* ------------------------------------------------------------------------ */
/*
 *   A SimpleLister provides simplified interfaces for creating formatted
 *   lists.  
 */
class SimpleLister: Lister
    /*
     *   Show a formatted list given a list of items.  This lets you create
     *   a formatted list from an item list without worrying about
     *   visibility or other factors that affect the full Lister
     *   interfaces. 
     */
    showSimpleList(lst)
    {
        showListAll(lst, 0, 0);
    }

    /* by default, everything given to a simple lister is listed */
    isListed(obj) { return true; }

    /*
     *   Format a simple list, but rather than displaying the result,
     *   return it as a string.  This simply displays the list as normal,
     *   but captures the output as a string and returns it. 
     */
    makeSimpleList(lst)
    {
        return mainOutputStream.captureOutput({: showSimpleList(lst) });
    }
;

/*
 *   objectLister is a concrete SimpleLister for listing simulation
 *   objects.
 */
objectLister: SimpleLister
;

/*
 *   stringLister is a concrete SimpleLister for formatting lists of
 *   strings.  To use this lister, pass lists of single-quoted strings
 *   (instead of simulation objects) to showSimpleList(), etc.  
 */
stringLister: SimpleLister
    /* show a list item - list items are strings, so simply 'say' them */
    showListItem(str, options, pov, infoTab) { say(str); }

    /* 
     *   get the cardinality of an arranged list (we need to override this
     *   because our items are strings, which don't have the normal object
     *   properties that would let us count cardinality the usual way) 
     */
    getArrangedListCardinality(singles, groups, groupTab)
    {
        return singles.length();
    }    
;


/* ------------------------------------------------------------------------ */
/*
 *   Plain lister - this lister doesn't show anything for an empty list,
 *   and doesn't show a list suffix or prefix. 
 */
plainLister: Lister
    /* show the prefix/suffix in wide mode */
    showListPrefixWide(itemCount, pov, parent) { }
    showListSuffixWide(itemCount, pov, parent) { }

    /* show the tall prefix */
    showListPrefixTall(itemCount, pov, parent) { }
;

/*
 *   Sub-lister for listing the contents of a group.  This lister shows a
 *   simple list with no prefix or suffix, and otherwise uses the
 *   characteristics of the parent lister.  
 */
class GroupSublister: object
    construct(parentLister, parentGroup)
    {
        /* remember the parent lister and group objects */
        self.parentLister = parentLister;
        self.parentGroup = parentGroup;
    }

    /* no prefix or suffix */
    showListPrefixWide(itemCount, pov, parent) { }
    showListSuffixWide(itemCount, pov, parent) { }
    showListPrefixTall(itemCount, pov, parent) { }

    /* show nothing when empty */
    showListEmpty(pov, parent) { }

    /*
     *   Show an item in the list.  Rather than going through the parent
     *   lister directly, we go through the parent group, so that it can
     *   customize the display of items in the group.  
     */
    showListItem(obj, options, pov, infoTab)
    {
        /* ask the parent group to handle it */
        parentGroup.showGroupItem(parentLister, obj, options, pov, infoTab);
    }

    /*
     *   Show a counted item in the group.  As with showListItem, we ask
     *   the parent group to do the work, so that it can customize the
     *   display if desired.  
     */
    showListItemCounted(lst, options, pov, infoTab)
    {
        /* ask the parent group to handle it */
        parentGroup.showGroupItemCounted(
            parentLister, lst, options, pov, infoTab);
    }

    /* delegate everything we don't explicitly handle to our parent lister */
    propNotDefined(prop, [args])
    {
        return delegated (getTopLister()).(prop)(args...);
    }

    /* get the top-level lister - returns my parent's top-level lister */
    getTopLister() { return parentLister.getTopLister(); }

    /* my parent lister */
    parentLister = nil

    /* my parent list group */
    parentGroup = nil
;

/*
 *   Paragraph lister: this shows its list items separated by paragraph
 *   breaks, with a paragraph break before the first item. 
 */
class ParagraphLister: Lister
    /* start the list with a paragraph break */
    showListPrefixWide(itemCount, pov, parent) { "<.p>"; }

    /* we show no list separators */
    showListSeparator(options, curItemNum, totalItems)
    {
        /* add a paragraph separator between items */
        if (curItemNum != totalItems)
            "<.p>";
    }
;

/*
 *   Lister for objects in a room description with special descriptions.
 *   Each special description gets its own paragraph, so this is based on
 *   the paragraph lister.  
 */
specialDescLister: ParagraphLister
    /* list everything */
    isListed(obj) { return true; }

    /* show a list item */
    showListItem(obj, options, pov, infoTab)
    {
        /* show the object's special description */
        obj.showSpecialDescWithInfo(infoTab[obj], pov);
    }

    /* use the object's special description grouper */
    listWith(obj) { return obj.specialDescListWith; }
;

/*
 *   Special description lister for the contents of an item being examined.
 *   This is similar to the regular specialDescLister, but shows the
 *   special descriptions of the contents of an object being described with
 *   "examine" or "look in," rather than of the entire location.  
 */
class SpecialDescContentsLister: ParagraphLister
    construct(cont)
    {
        /* remember the containing object being described */
        cont_ = cont;
    }

    /* list everything */
    isListed(obj) { return true; }

    /* show a list item */
    showListItem(obj, options, pov, infoTab)
    {
        /* show the object's special description in our container */
        obj.showSpecialDescInContentsWithInfo(infoTab[obj], pov, cont_);
    }

    /* use the object's special description grouper */
    listWith(obj) { return obj.specialDescListWith; }

    /* the containing object we're examining */
    cont_ = nil
;


/*
 *   Plain lister for actors.  This is the same as an ordinary
 *   plainLister, but ignores each object's isListed flag and lists it
 *   anyway. 
 */
plainActorLister: plainLister
    isListed(obj) { return true; }
;

/*
 *   Grouper for actors in a common posture and in a common location.  We
 *   create one of these per room per posture when we discover actors in
 *   the room during "look around" (or "examine" on a nested room).  This
 *   grouper lets us group actors like so: "Dan and Jane are sitting on
 *   the couch."  
 */
class RoomActorGrouper: ListGroup
    construct(location, posture)
    {
        self.location = location;
        self.posture = posture;
    }
    
    showGroupList(pov, lister, lst, options, indent, infoTab)
    {
        local cont;
        local outer;
        
        /* if the location isn't in the sense table, skip the whole list */
        if (infoTab[location] == nil)
            return;

        /* get the nominal posture container, if it's visible */
        cont = location.getNominalActorContainer(posture);
        if (cont != nil && !pov.canSee(cont))
            cont = nil;

        /* get the outermost visible enclosing location */
        outer = location.getOutermostVisibleRoom(pov);

        /* 
         *   Only mention the outermost location if it's remote and it's
         *   not the same as the nominal container.  (If the remote outer
         *   room is the same as the nominal container, it would be
         *   redundant to mention it as both the nominal and remote
         *   container.)  
         */
        if (outer == cont || pov.isIn(outer))
            outer = nil;

        /* create a sub-lister for the group */
        lister = createGroupSublister(lister);
        
        /* 
         *   show the list prefix message - use the nominal container if
         *   we can see it, otherwise generate a generic message 
         */
        if (cont != nil)
            cont.actorInGroupPrefix(pov, posture, outer, lst);
        else if (outer != nil)
            gLibMessages.actorThereGroupPrefix(pov, posture, outer, lst);
        else
            gLibMessages.actorHereGroupPrefix(posture, lst);

        /* list the actors' names as a plain list */
        plainActorLister.showList(pov, location, lst, options,
                                  indent, infoTab, self);

        /* add the suffix message */
        if (cont != nil)
            cont.actorInGroupSuffix(pov, posture, outer, lst);
        else if (outer != nil)
            gLibMessages.actorThereGroupSuffix(pov, posture, outer, lst);
        else
            gLibMessages.actorHereGroupSuffix(posture, lst);
    }
;

/* 
 *   Base class for inventory listers.  This lister uses a special listing
 *   method to show the items, so that items can be shown with special
 *   notations in an inventory list that might not appear in other types
 *   of listings.  
 */
class InventoryLister: Lister
    /* list items in inventory according to their isListedInInventory */
    isListed(obj) { return obj.isListedInInventory; }

    /*
     *   Show list items using the inventory name, which might differ from
     *   the regular nmae of the object.  
     */
    showListItem(obj, options, pov, infoTab)
        { obj.showInventoryItem(options, pov, infoTab); }

    showListItemCounted(lst, options, pov, infoTab)
        { lst[1].showInventoryItemCounted(lst, options, pov, infoTab); }

    /*
     *   Show contents of the items in the inventory.  We customize this
     *   so that we can differentiate inventory contents lists from other
     *   contents lists.  
     */
    showContentsList(pov, obj, options, indent, infoTab)
    {
        /* list the item's contents */
        obj.showInventoryContents(pov, obj.contentsLister, options,
                                  indent, infoTab);
    }

    /*
     *   Show the contents in-line, for an inventory listing. 
     */
    showInlineContentsList(pov, obj, options, indent, infoTab)
    {
        /* list the item's contents using its in-line lister */
        obj.showInventoryContents(pov, obj.inlineContentsLister,
                                  options, indent, infoTab);
    }
;

/*
 *   Base class for worn-inventory listers.  This lister uses a special
 *   listing method to show the items, so that items being worn are shown
 *   *without* the special '(being worn)' notation that might otherwise
 *   appear in inventory listings.  
 */
class WearingLister: InventoryLister
    /* show the list item using the "worn listing" name */
    showListItem(obj, options, pov, infoTab)
        { obj.showWornItem(options, pov, infoTab); }
    showListItemCounted(lst, options, pov, infoTab)
        { lst[1].showWornItemCounted(lst, options, pov, infoTab); }
;

/*
 *   "Divided" inventory lister.  In 'wide' mode, this shows inventory in
 *   two parts: the items being carried, and the items being worn.  (We use
 *   the standard single tree-style listing in 'tall' mode.)  
 */
class DividedInventoryLister: InventoryLister
    /*
     *   Show the list.  We completely override the main lister method so
     *   that we can show our two lists.  
     */
    showList(pov, parent, lst, options, indent, infoTab, parentGroup)
    {
        /* 
         *   If this is a 'tall' listing, use the normal listing style; for
         *   a 'wide' listing, use our special segregated style.  If we're
         *   being invoked recursively to show a contents listing, we
         *   similarly want to use the base handling. 
         */
        if ((options & (ListTall | ListContents)) != 0)
        {
            /* inherit the standard behavior */
            inherited(pov, parent, lst, options, indent, infoTab,
                      parentGroup);
        }
        else
        {
            local carryingLst, wearingLst;
            local carryingStr, wearingStr;

            /* divide the lists into 'carrying' and 'wearing' sublists */
            carryingLst = new Vector(32);
            wearingLst = new Vector(32);
            foreach (local cur in lst)
                (cur.isWornBy(parent) ? wearingLst : carryingLst).append(cur);

            /* generate and capture the 'carried' listing */
            carryingStr = outputManager.curOutputStream.captureOutput({:
                carryingLister.showList(pov, parent, carryingLst, options,
                                        indent, infoTab, parentGroup)});

            /* generate and capture the 'worn' listing */
            wearingStr = outputManager.curOutputStream.captureOutput({:
                wearingLister.showList(pov, parent, wearingLst, options,
                                       indent, infoTab, parentGroup)});

            /* generate the combined listing */
            showCombinedInventoryList(parent, carryingStr, wearingStr);

            /* 
             *   Now show the out-of-line contents for the whole list, if
             *   appropriate.  We save this until after showing both parts
             *   of the list, to keep the direct inventory parts together
             *   at the beginning of the output.  
             */
            if ((options & ListRecurse) != 0
                && indent == 0
                && (options & ListContents) == 0)
            {
                /* show the contents of each object we didn't list */
                showSeparateContents(pov, lst, options | ListContents,
                                     infoTab);
            }
        }
    }

    /*
     *   Show the combined listing.  This must be provided by each
     *   language-specific subclass.  The inputs are the results (strings)
     *   of the captured output of the sublistings of the items being
     *   carried and the items being worn.  These will be "raw" listings,
     *   without any prefix or suffix text.  This routine's job is to
     *   display the final output, adding the framing text.  
     */
    showCombinedInventoryList(parent, carrying, wearing) { }

    /* 
     *   The recommended maximum number of number of noun phrases to show
     *   in the single-sentence format.  This should be used by the
     *   showCombinedInventoryList() method to decide whether to display
     *   the combined listing as a single sentence or as two separate
     *   sentences.  
     */
    singleSentenceMaxNouns = 7

    /*
     *   Our associated sub-listers for items begin carried and worn,
     *   respectively.  We'll use these to list our sublist of items being
     *   worn.  
     */
    carryingLister = actorCarryingSublister
    wearingLister = actorWearingSublister
;

/*
 *   Base class for the inventory sub-lister for items being carried.  This
 *   is a minor specialization of the basic inventory lister; in this
 *   version, we omit any prefix, suffix, or empty messages, since we'll
 *   rely on the caller to combine our raw listing with the raw 'wearing'
 *   listing to form the full results.  
 *   
 *   This type of lister should normally only be used from within an
 *   inventory lister.  This type of lister assumes that it's part of a
 *   larger listing controlled externally; for example, we don't show
 *   out-of-line contents, since we assume the caller will be doing this.  
 */
class InventorySublister: InventoryLister
    /* don't show any prefix, suffix, or 'empty' messages */
    showListPrefixWide(itemCount, pov, parent) { }
    showListSuffixWide(itemCount, pov, parent) { }
    showListEmpty(pov, parent) { }

    /* don't show out-of-line contents */
    showSeparateContents(pov, lst, options, infoTab) { }
;

/*
 *   Base class for the inventory sub-lister for items being worn.  We use
 *   a special listing method to show these items, so that items being
 *   shown explicitly in a worn list can be shown differently from the way
 *   they would in a normal inventory list.  (For example, a worn item in a
 *   normal inventory list might show a "(worn)" indication, whereas it
 *   would not want to show a similar indication in a list of objects
 *   explicitly being worn.)
 *   
 *   This type of lister should normally only be used from within an
 *   inventory lister.  This type of lister assumes that it's part of a
 *   larger listing controlled externally; for example, we don't show
 *   out-of-line contents, since we assume the caller will be doing this.  
 */
class WearingSublister: WearingLister
    /* don't show any prefix, suffix, or 'empty' messages */
    showListPrefixWide(itemCount, pov, parent) { }
    showListSuffixWide(itemCount, pov, parent) { }
    showListEmpty(pov, parent) { }

    /* don't show out-of-line contents */
    showSeparateContents(pov, lst, options, infoTab) { }
;

/*
 *   The standard inventory sublisters.
 */
actorCarryingSublister: InventorySublister;
actorWearingSublister: WearingSublister;

/*
 *   Base class for contents listers.  This is used to list the contents
 *   of the objects that appear in top-level lists (a top-level list is
 *   the list of objects directly in a room that appears in a room
 *   description, or the list of items being carried in an INVENTORY
 *   command, or the direct contents of an object being examined). 
 */
class ContentsLister: Lister
;

/*
 *   Base class for description contents listers.  This is used to list
 *   the contents of an object when we examine the object, or when we
 *   explicitly LOOK IN the object.  
 */
class DescContentsLister: Lister
    /* 
     *   Use the explicit look-in flag for listing contents.  We might
     *   list items within an object on explicit examination of the item
     *   that we wouldn't list in a room or inventory list containing the
     *   item. 
     */
    isListed(obj) { return obj.isListedInContents; }
;

/*
 *   Base class for sense listers, which list the items that can be sensed
 *   for a command like "listen" or "smell". 
 */
class SenseLister: ParagraphLister
    /* show everything we're asked to list */
    isListed(obj) { return true; }

    /* show a counted list item */
    showListItemCounted(lst, options, pov, infoTab)
    {
        /* 
         *   simply show one item, without the count - non-visual senses
         *   don't distinguish numbers of items that are equivalent 
         */
        showListItem(lst[1], options, pov, infoTab);
    }
;

/*
 *   Room contents lister for things that can be heard.  
 */
roomListenLister: SenseLister
    /* list an item in a room if its isSoundListedInRoom is true */
    isListed(obj) { return obj.isSoundListedInRoom; }

    /* list an item */
    showListItem(obj, options, pov, infoTab)
    {
        /* show the "listen" list name for the item */
        obj.soundHereDesc();
    }
;

/*
 *   Lister for explicit "listen" action 
 */
listenActionLister: roomListenLister
    /* list everything in response to an explicit general LISTEN command */
    isListed(obj) { return true; }

    /* show an empty list */
    showListEmpty(pov, parent)
    {
        mainReport(&nothingToHearMsg);
    }

;

/*
 *   Room contents lister for things that can be smelled. 
 */
roomSmellLister: SenseLister
    /* list an item in a room if its isSmellListedInRoom is true */
    isListed(obj) { return obj.isSmellListedInRoom; }

    /* list an item */
    showListItem(obj, options, pov, infoTab)
    {
        /* show the "smell" list name for the item */
        obj.smellHereDesc();
    }
;

/*
 *   Lister for explicit "smell" action 
 */
smellActionLister: roomSmellLister
    /* list everything in response to an explicit general SMELL command */
    isListed(obj) { return true; }

    /* show an empty list */
    showListEmpty(pov, parent)
    {
        mainReport(&nothingToSmellMsg);
    }

;

/*
 *   Inventory lister for things that can be heard.  
 */
inventoryListenLister: SenseLister
    /* list an item */
    showListItem(obj, options, pov, infoTab)
    {
        /* show the "listen" list name for the item */
        obj.soundHereDesc();
    }
;

/*
 *   Inventory lister for things that can be smelled. 
 */
inventorySmellLister: SenseLister
    /* list an item */
    showListItem(obj, options, pov, infoTab)
    {
        /* show the "smell" list name for the item */
        obj.smellHereDesc();
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   List Group Interface.  An instance of this object is created for each
 *   set of objects that are to be grouped together.  
 */
class ListGroup: object
    /*
     *   Show a list of items from this group.  All of the items in the
     *   list will be members of this list group; we are to display a
     *   sentence fragment showing the items in the list, suitable for
     *   embedding in a larger list.
     *   
     *   'options', 'indent', and 'infoTab' have the same meaning as they
     *   do for showList().
     *   
     *   Note that we are not to display any separator before or after our
     *   list; the caller is responsible for that.  
     */
    showGroupList(pov, lister, lst, options, indent, infoTab) { }

    /*
     *   Show an item in the group's sublist.  The sublister calls this to
     *   display each item in the group when the group calls the sublister
     *   to display the group list.  By default, we simply let the
     *   sublister handle the request, which gives items in our group
     *   sublist the same appearance they would have had in the sublist to
     *   begin with.  We can customize this behavior to give our list
     *   items a different appearance special to the group sublist.
     *   
     *   Note that the same customization could be accomplished by
     *   creating a specialized subclass of GroupSublister in
     *   createGroupSublister(), and overriding showListItem() in the
     *   specialized GroupSublister subclass.  We use this mechanism as a
     *   convenience, so that a separate group sublister class doesn't
     *   have to be created simply to customize the display of group
     *   items.  
     */
    showGroupItem(sublister, obj, options, pov, infoTab)
    {
        /* by default, list using the regular sublister */
        sublister.showListItem(obj, options, pov, infoTab);
    }

    /*
     *   Show a counted item in our group list.  This is the counted item
     *   equivalent of showGroupItem.  
     */
    showGroupItemCounted(sublister, lst, options, pov, infoTab)
    {
        /* by default, list using the regular sublister */
        sublister.showListItemCounted(lst, options, pov, infoTab);
    }

    /* 
     *   Determine if showing the group list will introduce a sublist into
     *   an enclosing list.  This should return true if we will show a
     *   sublist without some kind of grouping, so that the caller knows
     *   to introduce some extra grouping into its enclosing list.  This
     *   should return nil if the sublist we display will be clearly set
     *   off in some way (for example, by being enclosed in parentheses). 
     */
    groupDisplaysSublist = true

    /*
     *   The minimum number of elements for which we should retain the
     *   group in a listing.  By default, we need two elements to display a
     *   group; any group with only one element is discarded, and the
     *   single element is moved into the 'singles' list.  This can be
     *   overridden to allow single-element groups to be retained.  In most
     *   cases, it's undesirable to retain single-element groups, but when
     *   grouping is used to partition a list into two or more fixed
     *   portions, single-element groups become desirable.  
     */
    minGroupSize = 2

    /*
     *   Determine the cardinality of the group listing, grammatically
     *   speaking.  This is the number of items that the group seems to be
     *   for the purposes of grammatical agreement.  For example, if the
     *   group is displayed as "$1.38 in change", it would be singular for
     *   grammatical agreement, hence would return 1 here; if it displays
     *   "five coins (two copper, three gold)," it would count as five
     *   items for grammatical agreement.
     *   
     *   For languages (like English) that grammatically distinguish
     *   number only between singular and plural, it is sufficient for
     *   this to return 1 for singular and anything higher for plural.
     *   For the sake of languages that make more elaborate number
     *   distinctions for grammatical agreement, though, this should
     *   return as accurate a count as is possible.
     *   
     *   By default, we return the number of items to be displayed in the
     *   list group.  This should be overridden when necessary, such as
     *   when the group message is singular in usage even if the list has
     *   multiple items (as in "$1.38 in change").  
     */
    groupCardinality(lister, lst) { return lst.length(); }

    /*
     *   Get the number of noun phrases this group will display.  This
     *   differs from the cardinality in that it doesn't matter how many
     *   *objects* the phrases represent; it only matters how many phrases
     *   are displayed.  For example, "five coins" has cardinality 5 but
     *   only displays one noun phrase.
     *   
     *   By default, we simply return the number of items in the group,
     *   since most groups individually list their items.
     */
    groupNounPhraseCount(lister, lst) { return lst.length(); }

    /*
     *   Create the group sublister.
     *   
     *   In most cases, when a group displays a list of the items in the
     *   group as a sublist, it will not want to use the same lister that
     *   was used to show the enclosing group, because the enclosing lister
     *   will usually have different prefix/suffix styles than the sublist.
     *   However, the group list and the enclosing list might share many
     *   other attributes, such as the style of name to use when displaying
     *   items in the list.  The default sublister we create,
     *   GroupSublister, is a hybrid that uses the enclosing lister's
     *   attributes except for a few, such as the prefix and suffix, that
     *   usually need to be changed for the sublist.
     *   
     *   This can be overridden to use a completely customized Lister
     *   object for the group list, if desired.  
     */
    createGroupSublister(parentLister)
    {
        /* create the standard group sublister by default */
        return new GroupSublister(parentLister, self);
    }
;

/*
 *   A "custom" List Group implementation.  This type of lister uses a
 *   completely custom message to show the group, without a need to
 *   recursively invoke a lister to list the individual elements.  The main
 *   difference between this and the base ListGroup is that the interface
 *   to the custom message generator is very simple - we can dispense with
 *   most of the numerous arguments that the base group message receives,
 *   since most of those arguments are there to allow recursive listing of
 *   the group list.
 *   
 *   This group type is intended mainly for cases where you want to display
 *   some sort of collective description of the group, rather than listing
 *   its members individually.  The whole point of the simple interface is
 *   that we don't pass the normal big pile of parameters because we won't
 *   be invoking a full sublisting.  Since we assume that this group won't
 *   itself look like a sublist, we set groupDisplaysSublist to nil by
 *   default.  This means that our presence in the overall list won't
 *   trigger the "long list" format (usually, this uses semicolons instead
 *   of commas) in the enclosing list.  If your custom group message does
 *   indeed look like a sublist (that is, it displays multiple items in a
 *   comma-separated list), you might want to change groupDisplaysSublist
 *   back to true so that the overall list is shown in the "long" format.  
 */
class ListGroupCustom: ListGroup
    showGroupList(pov, lister, lst, options, indent, infoTab)
    {
        /* simply show the custom message for the list */
        showGroupMsg(lst);
    }

    /* show the custom group message - subclasses should override */
    showGroupMsg(lst) { }

    /* assume our listing message doesn't look like a sublist */
    groupDisplaysSublist = nil
;

/*
 *   Sorted group list.  This is a list that simply displays its members
 *   in a specific sorting order. 
 */
class ListGroupSorted: ListGroup
    /*
     *   Show the group list 
     */
    showGroupList(pov, lister, lst, options, indent, infoTab)
    {
        /* put the list in sorted order */
        lst = sortListGroup(lst);

        /* create a sub-lister for the group */
        lister = createGroupSublister(lister);

        /* show the list */
        lister.showList(pov, nil, lst, options & ~ListContents,
                        indent, infoTab, self);
    }

    /*
     *   Sort the group list.  By default, if we have a
     *   compareGroupItems() method defined, we'll sort the list using
     *   that method; otherwise, we'll just return the list unchanged. 
     */
    sortListGroup(lst)
    {
        /* 
         *   if we have a compareGroupItems method, use it to sort the
         *   list; otherwise, just return the list in its current order 
         */
        if (propDefined(&compareGroupItems, PropDefAny))
            return lst.sort(SortAsc, {a, b: compareGroupItems(a, b)});
        else
            return lst;
    }

    /*
     *   Compare a pair of items from the group to determine their relative
     *   sorting order.  This should return 0 if the two items are at the
     *   same sorting order, a positive integer if the first item sorts
     *   after the second item, or a negative integer if the first item
     *   sorts before the second item.
     *   
     *   Note that we don't care about the return value beyond whether it's
     *   positive, negative, or zero.  This makes it especially easy to
     *   implement this method if the sorting order is determined by a
     *   property on each object that has an integer value: in this case
     *   you simply return the difference of the two property values, as in
     *   a.prop - b.prop.  This will have the effect of sorting the objects
     *   in ascending order of their 'prop' property values.  To sort in
     *   descending order of the same property, simply reverse the
     *   subtraction: b.prop - a.prop.
     *   
     *   When no implementation of this method is defined in the group
     *   object, sortListGroup won't bother sorting the list at all.
     *   
     *   By default, we don't implement this method.  Subclasses that want
     *   to impose a sorting order must implement the method.  
     */
    // compareGroupItems(a, b) { return a > b ? 1 : a == b ? 0 : -1; }
;

/*
 *   List Group implementation: parenthesized sublist.  Displays the
 *   number of items collectively, then displays the list of items in
 *   parentheses.
 *   
 *   Note that this is a ListGroupSorted subclass.  If our subclass
 *   defines a compareGroupItems() method, we'll show the list in the
 *   order specified by compareGroupItems().  
 */
class ListGroupParen: ListGroupSorted
    /* 
     *   show the group list 
     */
    showGroupList(pov, lister, lst, options, indent, infoTab)
    {
        /* sort the list group, if we have an ordering method defined */
        lst = sortListGroup(lst);

        /* create a sub-lister for the group */
        lister = createGroupSublister(lister);

        /* show the collective count of the object */
        showGroupCountName(lst);

        /* show the tall or wide sublist */
        if ((options & ListTall) != 0)
        {
            /* tall list - show the items as a sublist */
            "\n";
            lister.showList(pov, nil, lst, options & ~ListContents,
                            indent, infoTab, self);
        }
        else
        {
            /* wide list - add a space and a paren for the sublist */
            " (";

            /* show the sublist */
            lister.showList(pov, nil, lst, options & ~ListContents,
                            indent, infoTab, self);

            /* end the sublist */
            ")";
        }
    }

    /*
     *   Show the collective count for the list of objects.  By default,
     *   we'll simply display the countName of the first item in the list,
     *   on the assumption that each object has the same plural
     *   description.  However, in most cases this should be overridden to
     *   provide a more general collective name for the group. 
     */
    showGroupCountName(lst)
    {
        /* show the first item's countName */
        say(lst[1].countName(lst.length()));
    }

    /* we don't add a sublist, since we enclose our list in parentheses */
    groupDisplaysSublist = nil
;

/*
 *   List Group implementation: simple prefix/suffix lister.  Shows a
 *   prefix message, then shows the list, then shows a suffix message.
 *   
 *   Note that this is a ListGroupSorted subclass.  If our subclass
 *   defines a compareGroupItems() method, we'll show the list in the
 *   order specified by compareGroupItems().  
 */
class ListGroupPrefixSuffix: ListGroupSorted
    showGroupList(pov, lister, lst, options, indent, infoTab)
    {
        /* sort the list group, if we have an ordering method defined */
        lst = sortListGroup(lst);

        /* create a sub-lister for the group */
        lister = createGroupSublister(lister);

        /* show the prefix */
        showGroupPrefix(pov, lst);

        /* if we're in tall mode, start a new line */
        lister.showTallListNewline(options);

        /* show the list */
        lister.showList(pov, nil, lst, options & ~ListContents,
                        indent + 1, infoTab, self);

        /* show the suffix */
        showGroupSuffix(pov, lst);
    }

    /* show the prefix - we just show the groupPrefix message by default */
    showGroupPrefix(pov, lst) { groupPrefix; }

    /* show the suffix - we just show the groupSuffix message by default */
    showGroupSuffix(pov, lst) { groupSuffix; }

    /* 
     *   The prefix and suffix messages.  The showGroupPrefix and
     *   showGroupSuffix methods simply show these message properties.  We
     *   go through this two-step procedure for convenience: if the
     *   subclass doesn't need the POV and list parameters, it's less
     *   typing to just override these parameterless properties.  If the
     *   subclass needs to vary the message according to the POV or what's
     *   in the list, it can override the showGroupXxx methods instead.  
     */
    groupPrefix = ""
    groupSuffix = ""
;

/*
 *   Equivalent object list group.  This is the default listing group for
 *   equivalent items.  The Thing class creates an instance of this class
 *   during initialization for each set of equivalent items.  
 */
class ListGroupEquivalent: ListGroup
    showGroupList(pov, lister, lst, options, indent, infoTab)
    {
        /* show a count of the items */
        lister.showListItemCounted(lst, options, pov, infoTab);
    }

    /*
     *   An equivalence group displays only a single noun phrase to cover
     *   the entire group. 
     */
    groupNounPhraseCount(lister, lst) { return 1; }
     
    /* we display as a single item, so there's no sublist */
    groupDisplaysSublist = nil
;

