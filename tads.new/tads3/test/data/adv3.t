/* Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved. */
/*
 *   adv3.t - TADS 3 library main source file 
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Library Pre-Initializer.  This object performs the following
 *   initialization operations immediately after compilation is completed:
 *   
 *   - adds each defined Thing to its container's contents list
 *   
 *   - adds each defined Sense to the global sense list
 *   
 *   This object is named so that other libraries and/or user code can
 *   create initialization order dependencies upon it.  
 */
adv3LibPreinit: PreinitObject
    execute()
    {
        /*
         *   visit every Thing, and move each item into its location's
         *   contents list
         */
        forEachInstance(Thing, { obj: obj.initializeLocation() });

        /* 
         *   visit every Sense, and add each one into the global sense
         *   list 
         */
        forEachInstance(Sense, { obj: libGlobal.allSenses += obj });

        /* initialize each Actor */
        forEachInstance(Actor, { obj: obj.initializeActor() });

        /* initialize the globals */
        libGlobal.initialize();
    }
;

/*
 *   Library Initializer.  This object performs the following
 *   initialization operations each time the game is started:
 *   
 *   - sets up the library's default output function 
 */
adv3LibInit: InitObject
    execute()
    {
        /* set oup our default output function */
        t3SetSay(say);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   The library output processor. 
 */
say(val)
{
    local idx;

    /* if it's a string, check for substitutions */
    if (dataType(val) == TypeSString)
    {
        local tagPattern = '<nocase>[<]%.(/?[a-z]+)[>]';
        
        /* search for our special '<.xxx>' tags, and expand any we find */
        idx = rexSearch(tagPattern, val);
        while (idx != nil)
        {
            local xlat;
            local afterOfs;
            local afterStr;
            
            /* ask the formatter to translate it */
            xlat = libFormatter.translateTag(rexGroup(1)[3]);

            /* get the part of the string that follows the tag */
            afterOfs = idx[1] + idx[2];
            afterStr = val.substr(idx[1] + idx[2]);
                
            /* 
             *   if we got a translation, replace it; otherwise, leave the
             *   original text intact 
             */
            if (xlat != nil)
            {
                /* replace the tag with its translation */
                val = val.substr(1, idx[1] - 1) + xlat + afterStr;

                /* 
                 *   figure the offset of the remainder of the string in
                 *   the replaced version of the string - this is the
                 *   length of the original part up to the replacement
                 *   text plus the length of the replacement text 
                 */
                afterOfs = idx[1] + xlat.length();
            }

            /* 
             *   search for the next tag, considering only the part of
             *   the string following the replacement text - we do not
             *   want to re-scan the replacement text for tags 
             */
            idx = rexSearch(tagPattern, afterStr);
                
            /* 
             *   If we found it, adjust the starting index of the match to
             *   its position in the actual string.  Note that we do this
             *   by adding the OFFSET of the remainder of the string,
             *   which is 1 less than its INDEX, because idx[1] is already
             *   a string index.  (An offset is one less than an index
             *   because the index of the first character is 1.)  
             */
            if (idx != nil)
                idx[1] += afterOfs - 1;
        }
    }
    
    /* write the value to the console */
    tadsSay(val);
}

/*
 *   Library text formatter 
 */
libFormatter: object
    /*
     *   Translate a tag 
     */
    translateTag(tag)
    {
        /* make sure the tag is lower-case only */
        tag = tag.toLower();
        
        /* scan each tag/translation pair for the translation */
        for (local i = 1, local cnt = tagList.length() ; i < cnt ; i += 2)
        {
            /* 
             *   if this tag name matches, return its expansion; the tag
             *   is the first element of the pair, which is the current
             *   element, and the expansion is the second of the pair,
             *   thus the next element 
             */
            if (tag == tagList[i])
                return tagList[i + 1];
        }

        /* didn't find it */
        return nil;
    }

    /*
     *   our tag translation list - entries are in pairs: first the tag
     *   name, then the expansion 
     */
    tagList =
    [
        /* room name as part of room description */
        'roomname', '<p><b>',
        '/roomname', '</b>',

        /* body of room description */
        'roomdesc', '<br>\n',
        '/roomdesc', '',

        /* paragraph break within a room description */
        'roompara', '<p>\n'
    ]
;



/* ------------------------------------------------------------------------ */
/*
 *   Interface for showList() - this class is used to provide information
 *   to showList() on how the list should be displayed. 
 */
class ShowListInterface: object
    /* 
     *   Show the prefix for a 'wide' listing - this is a message that
     *   appears just before we start listing the objects.  'itemCount' is
     *   the number of items to be listed; the items might be grouped in
     *   the listing, so a list that comes out as "three boxes and two
     *   books" will have an itemCount of 5.  (The purpose of itemCount is
     *   to allow the message to have grammatical agreement in number.)
     *   
     *   This will never be called with an itemCount of zero, because we
     *   will instead use showListEmpty() to display an empty list.  
     */
    showListPrefixWide(itemCount, pov, parent) { }

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
     *   Show an empty list.  If the list to be displayed has no items at
     *   all, this is called instead of the prefix/suffix routines.  This
     *   can be left empty if no message is required for an empty list, or
     *   can display the complete message appropriate for an empty list
     *   (such as "You are carrying nothing").  
     */
    showListEmpty(pov, parent) { }
;

/*
 *   Show a list, showing all items in the list as though they were fully
 *   visible, regardless of their actual sense status.  
 */
showListAll(lister, lst, options, indent)
{
    local infoList;
    
    /* create an infoList with each item in full view */
    infoList = new Vector(lst.length());
    foreach (local cur in lst)
    {
        /* add a plain view sensory description to the info list */
        infoList += new SenseInfoListEntry(cur, transparent, nil, 3);
    }

    /* convert the info vector to a regular list */
    infoList = infoList.toList();

    /* show the list from the current global point of view */
    showList(getPOV(), nil, lister, lst, options, indent, infoList);
}

/*
 *   Display a list of items, grouping together members of a list group
 *   and equivalent items.  We will only list items for which isListed()
 *   returns true.
 *   
 *   'pov' is the point of view of the listing, which is usually an actor
 *   (and usually the player character actor).
 *   
 *   'parent' is the parent (container) of the list being shown.  This
 *   should be nil if the listed objects are not all within a single
 *   object.
 *   
 *   'lister' is an object implementing the methods in ShowListInterface -
 *   this object displays the before and after messages for the list.
 *   
 *   'lst' is the list of items to display.
 *   
 *   'options' gives a set of LIST_xxx option flags.
 *   
 *   'indent' gives the indentation level.  This is used only for "tall"
 *   lists (specified by including LIST_TALL in the options flags).  An
 *   indentation level of zero indicates no indentation.
 *   
 *   'infoList' the list of SenseInfoListEntry objects for all of the
 *   objects that can be sensed from the perspective of the actor
 *   performing the action that's causing the listing.  
 */
showList(pov, parent, lister, lst, options, indent, infoList)
{
    local cur;
    local i, cnt;
    local itemCount;
    local groups;
    local groupCount;
    local singles;
    local uniqueSingles;
    local sublists;
    local groupOptions;

    /* no groups yet */
    groupCount = 0;
    groups = new Vector(10);

    /* no singles yet */
    singles = new Vector(10);

    /* 
     *   First, scan the list to determine how many objects we're going to
     *   display. 
     */
    for (i = 1, cnt = lst.length(), itemCount = 0 ; i <= cnt ; ++i)
    {
        /* get this object into a local for easy reference */
        cur = lst[i];

        /* if the item isn't listed, skip it */
        if (!cur.isListed())
            continue;

        /* count the object */
        ++itemCount;

        /* check for groupings */
        if (cur.listWith != nil)
        {
        findPriorGroup:
            {
                /* 
                 *   look through our list of grouped objects for other
                 *   objects in the same list group 
                 */
                for (local j = 1 ; j <= groups.length() ; ++j)
                {
                    /* 
                     *   check the first object in this sublist (we only need
                     *   to check the first item in a sublist, since
                     *   everything in a sublist will have the same list group
                     *   - that's the way we construct the list) 
                     */
                    if (groups[j][1].listWith == cur.listWith)
                    {
                        /* found it - add this item to this group list */
                        groups[j] += cur;
                        
                        /* no need to look any further */
                        break findPriorGroup;
                    }
                }

                /* 
                 *   if we get here, it means that we didn't find this
                 *   item in an existing group - create a new group
                 *   sublist, and add this item as the first item in the
                 *   sublist 
                 */
                groups = groups.append([cur]);

                /* we have a new group */
                ++groupCount;
            }
        }
        else
        {
            /* 
             *   the object is not part of a group - add it to the list of
             *   items to be listed singly 
             */
            singles += cur;
        }
    }
        
    /*
     *   We now know how many items we're listing, so we can call the list
     *   interface object to set up the list.  If we're displaying nothing
     *   at all, just ask the interface object to display the message for
     *   an empty list, and we're done.
     */
    if (itemCount == 0)
    {
        lister.showListEmpty(pov, parent);
        return;
    }

    /* 
     *   If any of the groups have only one item, move them to the singles
     *   list.  Rebuild the groups list with the new list.  
     */
    if (groups.length() != 0)
    {
        /* scan the groups for single item entries */
        for (local i = 1 ; i <= groups.length() ; )
        {
            /* get this item */
            local cur = groups[i];
            
            /* if this group has only one member, it's effectively a single */
            if (cur.length() == 1)
            {
                /* move the single item in this group to the singles list */
                singles += cur[1];

                /* delete the entry from the groups list */
                groups.removeElementAt(i);
            }
            else
            {
                /* 
                 *   it's a legitimate group - keep it, and move on to the
                 *   next entry in the group list
                 */
                ++i;
            }
        }
    }


    /* 
     *   Check to see if we have one or more group sublists - if we do, we
     *   must use the "long" list format for our overall list, otherwise
     *   we can use the normal "short" list format.  The long list format
     *   uses semicolons to separate items.
     */
    for (i = 1, cnt = groups.length(), sublists = nil ; i <= cnt ; ++i)
    {
        /* 
         *   if this group's lister item displays a sublist, we must use
         *   the long format 
         */
        if (groups[i][1].listWith.groupDisplaysSublist)
        {
            /* note that we are using the long format */
            sublists = true;

            /* 
             *   one is enough to make us use the long format, so we need
             *   not look any further 
             */
            break;
        }
    }

    /* generate the prefix message */
    if ((options & LIST_TALL) != 0)
    {
        /* indent the prefix */
        showListIndent(options, indent);

        /* show the prefix */
        lister.showListPrefixTall(itemCount, pov, parent);

        /* go to a new line for the list contents */
        "\n";

        /* indent the items one level now, since we showed a prefix */
        ++indent;
    }
    else
    {
        /* show the prefix */
        lister.showListPrefixWide(itemCount, pov, parent);
    }

    /* 
     *   calculate the number of unique items in the singles list - we
     *   need to know this because we need to know how many items will
     *   follow the group list in order to figure out what type of
     *   separator to use after the last couple of group items 
     */
    uniqueSingles = getUniqueCount(singles, options);

    /* 
     *   regardless of whether we're adding long formatting to the main
     *   list, display the group sublists with whatever formatting we were
     *   originally using 
     */
    groupOptions = options;

    /* if we're using sublists, show the main list using long separators */
    if (sublists)
        options |= LIST_LONG;

    /* show each group */
    for (i = 1, cnt = groups.length() ; i <= cnt ; ++i)
    {
        /* in tall mode, indent before the group description */
        showListIndent(options, indent);
        
        /* ask the listGroup object to display the list */
        groups[i][1].listWith.
            showGroupList(pov, lister, groups[i], groupOptions,
                          indent, infoList);
        
        /* 
         *   Show the post-group separator.  For the purposes of figuring
         *   out what separator to use here, each group and each single
         *   item counts as one item. 
         */
        showListSeparator(options, i, cnt + uniqueSingles);
    }

    /* show the individual items in the list */
    showListSimple(pov, lister, singles, options, indent, cnt, infoList);

    /* 
     *   generate the suffix message if we're in 'wide' mode ('tall' lists
     *   have no suffix; the format only allows for a prefix) 
     */
    if ((options & LIST_TALL) == 0)
        lister.showListSuffixWide(itemCount, pov, parent);
}

/*
 *   Show a list indent if necessary.  If LIST_TALL is included in the
 *   options, we'll indent to the given level; otherwise we'll do nothing.
 */
showListIndent(options, indent)
{
    /* show the indent only if we're in "tall" mode */
    if ((options & LIST_TALL) != 0)
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
    if ((options & LIST_TALL) != 0)
        "\n";
}

/*
 *   Show a simple list, grouping together equivalent items.  This
 *   "simple" list formatter doesn't pay any attention to list groups.
 *   
 *   'prevCnt' is the number of items already displayed, if anything has
 *   already been displayed for this list.  This should be zero if this
 *   will display the entire list.  
 */
showListSimple(pov, lister, lst, options, indent, prevCnt, infoList)
{
    local i;
    local cnt;
    local dispCount;
    local uniqueCount;

    /* count the unique entries in the list */
    uniqueCount = getUniqueCount(lst, options) + prevCnt;

    /* display the items */
dispLoop:
    for (i = 1, cnt = lst.length(), dispCount = prevCnt ; i <= cnt ; ++i)
    {
        local cur;
        local curSenseInfo;
        local dispSingle;

        /* get the item */
        cur = lst[i];

        /* presume we'll display it as a single item */
        dispSingle = true;
        
        /* 
         *   If this is an item with equivalents, search the list for
         *   other items that match.  If this is the first one, list it,
         *   with the count of matching items.  If this isn't the first
         *   one, don't bother listing it, since we've already included it
         *   in the earlier item's listing. 
         */
        if (cur.isEquivalent)
        {
            local equivCount;

            /* search for an earlier instance of the item in the list */
            for (local j = 1 ; j < i ; ++j)
            {
                /* 
                 *   check to see if this item lists the same as the first
                 *   item we found - if so, we can skip to the next item
                 *   in the main loop, since we've already listed this
                 *   item 
                 */
                if (lst[j].isEquivalent
                    && lst[j].isListEquivalent(cur, options))
                    continue dispLoop;
            }

            /* 
             *   we didn't find it, so it hasn't been listed yet - count
             *   the number of instances of this item in the rest of the
             *   list, so we can include all of them in the count of this
             *   item 
             */
            for (local j = i + 1, equivCount = 1 ; j <= cnt ; ++j)
            {
                /* if it matches, count it */
                if (lst[j].isEquivalent
                    && lst[j].isListEquivalent(cur, options))
                    ++equivCount;
            }

            /* 
             *   if we found more than one such item, display the entire
             *   set of them with a count 
             */
            if (equivCount > 1)
            {
                /* show the list indent if necessary */
                showListIndent(options, indent);

                /* get the sense information for this item */
                curSenseInfo = infoList.valWhich({x: x.obj == cur});
                
                /* display the whole group */
                cur.showListItemCounted(equivCount, options,
                                        pov, curSenseInfo);
                
                /* note that we don't need to display it as a single */
                dispSingle = nil;
            }
        }

        /* if necessary, display the item as a single item */
        if (dispSingle)
        {
            /* show the list indent if necessary */
            showListIndent(options, indent);
            
            /* get the sense information for this item */
            curSenseInfo = infoList.valWhich({x: x.obj == cur});
                
            /* show the item */
            cur.showListItem(options, pov, curSenseInfo);
        }

        /* count the item displayed */
        ++dispCount;

        /* show the list separator */
        showListSeparator(options, dispCount, uniqueCount);

        /* if this is a tall recursive list, show the item's contents */
        if ((options & LIST_TALL) != 0 && (options & LIST_RECURSE) != 0)
            cur.showObjectContents(pov, lister, options,
                                   indent + 1, infoList);
    }
}

/*
 *   Show a list separator after displaying an item.  curItemNum is the
 *   number of the item just displayed (1 is the first item), and
 *   totalItems is the total number of items that will be displayed in the
 *   list.  
 */
showListSeparator(options, curItemNum, totalItems)
{
    local useLong = ((options & LIST_LONG) != 0);

    /* if this is a tall list, the separator is simply a newline */
    if ((options & LIST_TALL) != 0)
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
         *   need to use the special last-item separator.  If we're only
         *   displaying two items total, we use an even more special
         *   separator.  
         */
        if (totalItems == 2)
        {
            /* use the two-item separator */
            if (useLong)
                libMessages.longListSepTwo;
            else
                libMessages.listSepTwo;
        }
        else
        {
            /* use the normal last-item separator */
            if (useLong)
                libMessages.longListSepEnd;
            else
                libMessages.listSepEnd;
        }
    }
    else
    {
        /* we're in the middle of the list - display the normal separator */
        if (useLong)
            libMessages.longListSepMiddle;
        else
            libMessages.listSepMiddle;
    }
}

/*
 *   Calculate the number of unique items in the list. 
 */
getUniqueCount(lst, options)
{
    local uniqueCount;
    
    /* run through the list and count the unique entries */
    for (local i = 1, local cnt = lst.length(), uniqueCount = 0 ;
         i <= cnt ; ++i)
    {
        local cur;

        /* get this item into a local for easier access */
        cur = lst[i];
        
        /* tentatively assume this item is unique */
        ++uniqueCount;

        /* 
         *   if it's marked as interchangeable with other items of its
         *   class, note its class list, then try to find other objects
         *   marked the same way 
         */
        if (cur.isEquivalent)
        {
            /* look for equivalents among the previous items */
            for (local j = 1 ; j < i ; ++j)
            {
                /* check for a match */
                if (lst[j].isEquivalent
                     && lst[j].isListEquivalent(cur, options))
                {
                    /* it doesn't count as unique after all */
                    --uniqueCount;

                    /* no need to look any further */
                    break;
                }
            }
        }
    }

    /* return the number of unique items we counted */
    return uniqueCount;
}

/* ------------------------------------------------------------------------ */
/*
 *   Show the contents of a set of items.  For each item in the list, if
 *   the object's contents can be sensed, we'll display them.  We don't
 *   display anything for the objects in the lists themselves - only for
 *   the contents of the objects in the list.
 *   
 *   'pov' is the point of view, which is usually an actor (and usually
 *   the player character actor).
 *   
 *   'lst' is the list of objects to display.
 *   
 *   'options' is the set of flags that we'll pass to showList(), and has
 *   the same meaning as for that function.
 *   
 *   'infoList' is a list of SenseInfoListEntry objects giving the sense
 *   information for all of the objects that the actor to whom we're
 *   showing the contents listing can sense.  
 */
showContentsList(pov, lst, options, indent, infoList)
{
    /* go through each item and show its contents */
    foreach (local cur in lst)
        cur.showObjectContents(pov, cur.contentsLister, options,
                               indent, infoList);
}

/* ------------------------------------------------------------------------ */
/*
 *   Sense Info List entry.  Thing.senseInfoList() returns a list of these
 *   objects to provide full sensory detail on the objects within range of
 *   a sense.  
 */
class SenseInfoListEntry: object
    construct(obj, trans, obstructor, ambient)
    {
        self.obj = obj;
        self.trans = trans;
        self.obstructor = obstructor;
        self.ambient = ambient;
    }
    
    /* the object being sensed */
    obj = nil

    /* the transparency from the point of view to this object */
    trans = nil

    /* the obstructor that introduces a non-transparent value of trans */
    obstructor = nil

    /* the ambient sense energy level at this object */
    ambient = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   List Group Interface.  This object is instantiated for each set of
 *   non-equivalent items that should be grouped together in listings.  
 */
class ListGroup: object
    /*
     *   Show a list of items from this group.  All of the items in the
     *   list will be members of this list group; we are to display a
     *   sentence fragment showing the items in the list, suitable for
     *   embedding in a larger list.
     *   
     *   'options', 'indent', and 'infoList' have the same meaning as they
     *   do for showList().
     *   
     *   Note that we are not to display any separator before or after our
     *   list; the caller is responsible for that.  
     */
    showGroupList(pov, lister, lst, options, indent, infoList) { }

    /* 
     *   Determine if showing the group list will introduce a sublist into
     *   an enclosing list.  This should return true if we will show a
     *   sublist without some kind of grouping, so that the caller knows
     *   to introduce some extra grouping into its enclosing list.  This
     *   should return nil if the sublist we display will be clearly set
     *   off in some way (for example, by being enclosed in parentheses). 
     */
    groupDisplaysSublist = true
;

/*
 *   List Group implementation: parenthesized sublist.  Displays the
 *   number of items collectively, then displays the list of items in
 *   parentheses. 
 */
class ListGroupParen: ListGroup
    /* 
     *   show the group list 
     */
    showGroupList(pov, lister, lst, options, indent, infoList)
    {
        /* show the collective count of the object */
        showGroupCountDesc(lst);

        /* show the tall or wide sublist */
        if ((options & LIST_TALL) != 0)
        {
            /* tall list - show the items as a sublist */
            "\n";
            showListSimple(pov, lister, lst, options | LIST_GROUP, indent,
                           0, infoList);
        }
        else
        {
            /* wide list - add a space and a paren for the sublist */
            " (";

            /* show the sublist */
            showListSimple(pov, lister, lst, options | LIST_GROUP, indent,
                           0, infoList);

            /* end the sublist */
            ")";
        }
    }

    /*
     *   Show the collective count for the list of objects.  By default,
     *   we'll simply display the countDesc of the first item in the list,
     *   on the assumption that each object has the same plural
     *   description.  However, in most cases this should be overridden to
     *   provide a more general collective name for the group. 
     */
    showGroupCountDesc(lst)
    {
        /* show the first item's countDesc */
        lst[1].countDesc(lst.length());
    }

    /* we don't add a sublist, since we enclose our list in parentheses */
    groupDisplaysSublist = nil
;

/*
 *   List Group implementation: simple prefix/suffix lister.  Shows a
 *   prefix message, then shows the list, then shows a suffix message.
 */
class ListGroupPrefixSuffix: ListGroup
    showGroupList(pov, lister, lst, options, indent, infoList)
    {
        /* show the prefix */
        showGroupPrefix;

        /* if we're in tall mode, start a new line */
        showTallListNewline(options);

        /* show the list */
        showListSimple(pov, lister, lst, options | LIST_GROUP, indent + 1,
                       0, infoList);

        /* show the suffix */
        showGroupSuffix;
    }

    /* the prefix - subclasses should override this if desired */
    showGroupPrefix = ""

    /* the suffix */
    showGroupSuffix = ""
;

/* ------------------------------------------------------------------------ */
/*
 *   Library global variables 
 */
libGlobal: object
    /* initialize the library globals */
    initialize()
    {
        /* set up a vector for our point of view stack */
        povStack = new Vector(32);

        /* set up the full-score vectors */
        fullScorePoints = new Vector(32);
        fullScoreDesc = new Vector(32);
    }
    
    /*
     *   List of all of the senses.  The library pre-initializer will load
     *   this list with a reference to each instance of class Sense. 
     */
    allSenses = []

    /*
     *   The current player character 
     */
    playerChar = nil

    /*
     *   The current perspective object.  This is usually the actor
     *   performing the current command. 
     */
    pointOfView = nil

    /*
     *   The stack of point of view objects.  We initialize this during
     *   start-up to a vector.  The last element of the vector is the most
     *   recent point of view after the current point of view.  
     */
    povStack = nil

    /*
     *   Vectors for the score.  The first stores the number of points for
     *   each item scored, and the second stores the description
     *   corresponding to each point count.  We'll allocate these at
     *   startup.  
     */
    fullScorePoints = nil
    fullScoreDesc = nil

    /* the total number of points scored so far */
    totalScore = 0

    /* 
     *   the maximum number of points possible - the game should set this
     *   to the appropriate value at startup 
     */
    maxScore = 100

    /* the total number of turns so far */
    totalTurns = 0
;

/* ------------------------------------------------------------------------ */
/*
 *   An Achievement is an object used to award points in the score.  For
 *   most purposes, an achievement can be described simply by a string,
 *   but the Achievement object provides more flexibility in describing
 *   combined scores when a similar set of achievements are to be grouped.
 */
class Achievement: object
    /* 
     *   The number of times the achievement has been awarded.  Each time
     *   the achievement is passed to addToScore(), this is incremented.
     *   Note that this is distinct from the number of points.  
     */
    scoreCount = 0
    
    /* 
     *   Describe the achievement - this must display a string explaining
     *   the reason the points associated with this achievement were
     *   awarded.
     *   
     *   Note that this description can make use of the scoreCount
     *   information to show different descriptions depending on how many
     *   times the item has scored.  For example, an achievement for
     *   finding various treasure items might want to display "finding a
     *   treasure" if only one treasure was found and "finding five
     *   treasures" if five were found.
     *   
     *   In some cases, it might be desirable to keep track of additional
     *   custom information, and use that information in generating the
     *   description.  For example, the game might keep a list of
     *   treasures found with the achievement, adding to the list each
     *   time the achievement is scored, and displaying the contents of
     *   the list when the description is shown.  
     */
    lDesc = ""
;

/*
 *   Add to the score.  'points' is the number of points to add to the
 *   score, and 'desc' is a string describing the reason the points are
 *   being awarded, or an Achievement object describing the points.
 *   
 *   We keep a list of each unique description.  If 'desc' is already in
 *   this list, we'll simply add the given number of points to the
 *   existing entry for the same description.
 *   
 *   Note that, if 'desc' is an Achievement object, it will match a
 *   previous item only if it's exactly the same Achievement instance.  
 */
addToScore(points, desc)
{
    local idx;
    
    /* if 'desc' is an Achievement item, increase its use count */
    if (desc.ofKind(Achievement))
        desc.scoreCount++;

    /* add the points to the total */
    libGlobal.totalScore += points;

    /* try to find a matching achievement in our list of past score items */
    idx = libGlobal.fullScoreDesc.indexOf(desc);

    /* if we found it, merely increase the number of points for the item */
    if (idx != nil)
    {
        /* 
         *   it's already in there - simply combine the points into the
         *   existing entry
         */
        libGlobal.fullScorePoints[idx] += points;
    }
    else
    {
        /* it's not in the list yet - add it */
        libGlobal.fullScoreDesc.append(desc);
        libGlobal.fullScorePoints.append(points);
    }
}

/*
 *   Show the simple score
 */
showScore()
{
    /* show the basic score statistics */
    libMessages.showScoreMessage(libGlobal.totalScore, libGlobal.maxScore,
                                 libGlobal.totalTurns);

    /* show the score ranking */
    showScoreRank(libGlobal.totalScore);
}

/* 
 *   show the score rank message 
 */
showScoreRank(points)
{
    local idx;

    /* if there's no rank table, skip the ranking */
    if (libMessages.scoreRankTable == nil)
        return;
    
    /*
     *   find the last item for which our score is at least the minimum -
     *   the table is in ascending order of minimum score, so we want the
     *   last item for which our score is sufficient 
     */
    idx = libMessages.scoreRankTable.lastIndexWhich({x: points >= x[1]});
    
    /* if we didn't find an item, use the first by default */
    if (idx == nil)
        idx = 1;
    
    /* show the description from the item we found */
    libMessages.showScoreRankMessage(libMessages.scoreRankTable[idx][2]);
}

/*
 *   Display the full score 
 */
showFullScore()
{
    /* show the basic score statistics */
    showScore();

    /* if any points have been awarded, show the full score */
    if (libGlobal.totalScore != 0)
    {
        /* show a blank line before the full list */
        "\b";

        /* show the list prefix */
        libMessages.showFullScorePrefix;
        "\n";

        /* show each item in the full score list */
        for (local i = 1, local cnt = libGlobal.fullScoreDesc.length() ;
             i <= cnt ; ++i)
        {
            local desc;
            
            /* show the number of points */
            libMessages.fullScoreItemPoints(libGlobal.fullScorePoints[i]);

            /* 
             *   if this description item is an Achivement, call its lDesc
             *   to display it; otherwise, it must simply be a string, so
             *   we can display it directly 
             */
            desc = libGlobal.fullScoreDesc[i];
            if (desc.ofKind(Achievement))
            {
                /* it's an Achievement - use its lDesc to display it */
                desc.lDesc;
            }
            else
            {
                /* it's simply a string - display it directly */
                say(desc);
            }

            /* one item per line - end the line here */
            "\n";
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Get the current point of view.  This is the actor object that should
 *   be used when generating names of objects.  
 */
getPOV()
{
    return libGlobal.pointOfView;
}

/*
 *   Change the point of view without altering the point-of-view stack 
 */
setPOV(pov)
{
    /* set the new point of view */
    libGlobal.pointOfView = pov;
}

/*
 *   Set the root point of view.  This doesn't affect the current point of
 *   view unless there is no current point of view; this merely sets the
 *   outermost default point of view.  
 */
setRootPOV(pov)
{
    /* 
     *   if there's nothing in the stacked list, set the current point of
     *   view; otherwise, just set the innermost stacked element 
     */
    if (libGlobal.povStack.length() == 0)
    {
        /* there is no point of view, so set the current point of view */
        libGlobal.pointOfView = pov;
    }
    else
    {
        /* set the innermost stacked point of view */
        libGlobal.povStack[1] = pov;
    }
}

/*
 *   Push the current point of view 
 */
pushPOV(pov)
{
    /* stack the current one */
    libGlobal.povStack += libGlobal.pointOfView;

    /* set the new point of view */
    setPOV(pov);
}

/*
 *   Pop the most recent point of view pushed 
 */
popPOV()
{
    local len;
    
    /* check if there's anything left on the stack */
    len = libGlobal.povStack.length();
    if (len != 0)
    {
        /* take the most recent element off the stack */
        libGlobal.pointOfView = libGlobal.povStack[len];

        /* take it off the stack */
        libGlobal.povStack.removeElementAt(len);
    }
    else
    {
        /* nothing on the stack - clear the point of view */
        libGlobal.pointOfView = nil;
    }
}

/*
 *   Clear the point of view and all stacked elements
 */
clearPOV()
{
    local len;
    
    /* forget the current point of view */
    setPOV(nil);

    /* drop everything on the stack */
    len = libGlobal.povStack.length();
    libGlobal.povStack.removeRange(1, len);
}

/*
 *   Call a function from a point of view.  We'll set the new point of
 *   view, call the function with the given arguments, then restore the
 *   original point of view. 
 */
callFromPOV(pov, funcToCall, [args])
{
    /* push the new point of view */
    pushPOV(pov);

    /* make sure we pop the point of view no matter how we leave */
    try
    {
        /* call the function */
        (funcToCall)(args...);
    }
    finally
    {
        /* restore the enclosing point of view on the way out */
        popPOV();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   "Add" two transparency levels, yielding a new transparency level.
 *   This function can be used to determine the result of passing a sense
 *   through multiple layers of material.  
 */
transparencyAdd(a, b)
{
    /* transparent + x -> x for all x */
    if (a == transparent)
        return b;
    if (b == transparent)
        return a;

    /* opaque + x -> opaque for all x */
    if (a == opaque || b == opaque)
        return opaque;

    /*
     *   distant + distant, obscured + obscured, and distant + obscured
     *   all yield opaque - since neither is transparent or opaque, both
     *   must be obscured or distant, so the result must be opaque
     */
    return opaque;
}

/*
 *   Compare two transparency levels to determine which one is more
 *   transparent.  Returns 0 if the two levels are equally transparent, 1
 *   if the first one is more transparent, and -1 if the second one is
 *   more transparent.  The comparison follows this rule:
 *   
 *   transparent > distant == obscured > opaque 
 */
transparencyCompare(a, b)
{
    /*
     *   for the purposes of the comparison, consider obscured to be
     *   identical to distant
     */
    if (a == obscured)
        a = distant;
    if (b == obscured)
        b = distant;

    /* if they're the same, return zero to so indicate */
    if (a == b)
        return 0;

    /*
     *   We know they're not equal, so if one is transparent, then the
     *   other one isn't.  Thus, if either one is transparent, it's the
     *   winner.
     */
    if (a == transparent)
        return 1;
    if (b == transparent)
        return -1;

    /*
     *   We now know neither one is transparent, and we've already
     *   transformed obscured into distant, so the only possible values
     *   remaining are distant and opaque.  We know also they're not
     *   equal, because we would have already returned if that were the
     *   case.  So, we can conclude that one must be distant and the other
     *   must be opaque.  Hence, the one that's opaque is the less
     *   transparent one.
     */
    if (a == opaque)
        return -1;
    else
        return 1;
}

/*
 *   Given a brightness level and a transparency level, compute the
 *   brightness as modified by the transparency level. 
 */
adjustBrightness(br, trans)
{
    switch(trans)
    {
    case transparent:
        /* transparent medium - this doesn't modify brightness at all */
        return br;

    case obscured:
    case distant:
        /* 
         *   Distant or obscured.  We reduce self-illuminating light
         *   (level 1) and dim light (level 2) to nothing (level 0), we
         *   leave nothing as nothing (obviously), and we reduce all other
         *   levels one step.  So, everything below level 3 goes to 0, and
         *   everything at or above level 3 gets decremented by 1.  
         */
        return (br >= 3 ? br - 1 : 0);

    case opaque:
        /* opaque medium - nothing makes it through */
        return 0;

    default:
        /* shouldn't get to other cases */
        return nil;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Material: the base class for library objects that specify the way
 *   senses pass through objects.  
 */
class Material: object
    /*
     *   Determine how a sense passes through the material.  We'll return
     *   a transparency level.  (Individual materials should not need to
     *   override this method, since it simply dispatches to the various
     *   xxxThru methods.)
     */
    senseThru(sense)
    {
        /* dispatch to the xxxThru method for the sense */
        return self.(sense.thruProp);
    }

    /*
     *   For each sense, each material must define an appropriate xxxThru
     *   property that returns the transparency level for that sense
     *   through the material.  Any xxxThru property not defined in an
     *   individual material defaults to opaque.
     */
    seeThru = opaque
    hearThru = opaque
    smellThru = opaque
    touchThru = opaque
;

/*
 *   Adventium is the basic stuff of the game universe.  This is the
 *   default material for any object that doesn't specify a different
 *   material.  This type of material is opaque to all senses.  
 */
adventium: Material
    seeThru = opaque
    hearThru = opaque
    smellThru = opaque
    touchThru = opaque
;

/*
 *   Paper is opaque to sight and touch, but allows sound and smell to
 *   pass.  
 */
paper: Material
    seeThru = opaque
    hearThru = transparent
    smellThru = transparent
    touchThru = opaque
;

/*
 *   Glass is transparent to light, but opaque to touch, sound, and smell.
 *   
 */
glass: Material
    seeThru = transparent
    hearThru = opaque
    smellThru = opaque
    touchThru = opaque
;

/*
 *   Fine Mesh is transparent to all senses except touch.  
 */
fineMesh: Material
    seeThru = transparent
    hearThru = transparent
    smellThru = transparent
    touchThru = opaque
;

/*
 *   Coarse Mesh is transparent to all senses, including touch, but
 *   doesn't allow large objects to pass through.  
 */
coarseMesh: Material
    seeThru = transparent
    hearThru = transparent
    smellThru = transparent
    touchThru = transparent
;

/* ------------------------------------------------------------------------ */
/*
 *   Sense: the basic class for senses.  
 */
class Sense: object
    /*
     *   Each sense must define the property thruProp as a property
     *   pointer giving the xxxThru property for the sense.  The xxxThru
     *   property is the property of a material which determines how the
     *   sense passes through that material.  
     */
    thruProp = nil

    /*
     *   Each sense must define the property sizeProp as a property
     *   pointer giving the xxxSize property for the sense.  The xxxSize
     *   property is the property of a Thing which determines how "large"
     *   the object is with respect to the sense.  For example, sightSize
     *   indicates how large the object is visually, while soundSize
     *   indicates how loud the object is.
     *   
     *   The purpose of an object's size in a given sense is to determine
     *   how well the object can be sensed through an obscuring medium or
     *   at a distance.  
     */
    sizeProp = nil

    /*
     *   Each sense must define the property presenceProp as a property
     *   pointer giving the xxxPresence property for the sense.  The
     *   xxxPresence property is the property of a Thing which determines
     *   whether or not the object has a "presence" in this sense, which
     *   is to say whether or not the object is emitting any detectable
     *   sensory data for the sense.  For example, soundPresence indicates
     *   whether or not a Thing is making any noise.  
     */
    presenceProp = nil

    /*
     *   Each sense can define this property to specify a property pointer
     *   used to define a Thing's "ambient" energy emissions.  Senses
     *   which do not use ambient energy should define this to nil.
     *   
     *   Some senses work only on directly emitted sensory data; human
     *   hearing, for example, has no (at least effectively no) use for
     *   reflected sound, and can sense objects only by the sounds they're
     *   actually emitting.  Sight, on the other hand, can make use not
     *   only of light emitted by an object but of light reflected by the
     *   object.  So, sight defines an ambience property, whereas hearing,
     *   touch, and smell do not.  
     */
    ambienceProp = nil

    /*
     *   Determine if, in general, the given object can be sensed under
     *   the given conditions.  Returns true if so, nil if not.  By
     *   default, if the ambient level is zero, we'll return nil;
     *   otherwise, if the transparency level is 'transparent', we'll
     *   return true; otherwise, we'll consult the object's size:
     *   
     *   - Small objects cannot be sensed under less than transparent
     *   conditions.
     *   
     *   - Medium or large objects can be sensed in any conditions other
     *   than opaque.  
     */
    canSenseObj(obj, trans, ambient)
    {
        /* if the ambient energy level is zero, we can't sense it */
        if (ambient == 0)
            return nil;

        /* check the transparency level */
        switch(trans)
        {
        case transparent:
            /* we can always sense under transparent conditions */
            return true;

        case distant:
        case obscured:
            /* 
             *   we can only sense medium and large objects under less
             *   than transparent conditions 
             */
            return obj.(self.sizeProp) != small;

        default:
            /* we can never sense under other conditions */
            return nil;
        }
    }
;

/*
 *   The senses.  We define sight, sound, smell, and touch.  We do not
 *   define a separate sense for taste, since it would add nothing to our
 *   model: you can taste something if and only if you can touch it.
 *   
 *   To add a new sense, you must do the following:
 *   
 *   - Define the sense object itself, in parallel to the senses defined
 *   below.
 *   
 *   - Modify class Material to set the default transparency level for
 *   this sense by defining the property xxxThru - for most senses, the
 *   default transparency level is 'opaque', but you must decide on the
 *   appropriate default for your new sense.
 *   
 *   - Modify class Thing to set the default xxxSize setting, if desired.
 *   
 *   - Modify class Thing to set the default xxxPresence setting, if
 *   desired.
 *   
 *   - Modify each instance of class 'Material' that should have a
 *   non-default transparency for the sense by defining the property
 *   xxxThru for the material.
 *   
 *   - Modify class Actor to add the sense to the default mySenses list;
 *   this is only necessary if the sense is one that all actors should
 *   have by default.  
 */

sight: Sense
    thruProp = &seeThru
    sizeProp = &sightSize
    presenceProp = &sightPresence
    ambienceProp = &brightness
;

sound: Sense
    thruProp = &hearThru
    sizeProp = &soundSize
    presenceProp = &soundPresence
;

smell: Sense
    thruProp = &smellThru
    sizeProp = &smellSize
    presenceProp = &smellPresence
;

touch: Sense
    thruProp = &touchThru
    sizeProp = &touchSize
    presenceProp = &touchPresence

    /*
     *   Override canSenseObj for touch.  Unlike other senses, touch
     *   requires physical contact with an object, so it cannot operate at
     *   a distance, regardless of the size of an object.  
     */
    canSenseObj(obj, trans, ambient)
    {
        /* if it's distant, we can't sense the object no matter how large */
        if (trans == distant)
            return nil;

        /* for other cases, inherit the default handling */
        return inherited.canSenseObj(obj, trans, ambient);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Thing: the basic class for game objects.  An object of this class
 *   represents a physical object in the simulation.
 *   
 *   The LangThing base class defines some language-specific methods and
 *   properties, which are separated out from Thing to make it easy for a
 *   translator to substitute a translated implementation without changing
 *   the rest of the library code.  
 */
class Thing: LangThing
    /*
     *   If we define a non-nil initDesc, this property will be called to
     *   describe the object in room listings until the object is first
     *   moved to a new location.  By default, objects don't have initial
     *   descriptions.  
     */
    initDesc = nil

    /*
     *   Flag: I've been moved out of my initial location.  Whenever we
     *   move the object to a new location, we'll set this to true.  
     */
    isMoved = nil

    /*
     *   Determine if I should be described using my initial description.
     *   This returns true if I have an initial description that isn't
     *   nil, and I have never been moved out of my initial location.  If
     *   this returns nil, the object should be described in room
     *   descriptions using the ordinary generated message.  
     */
    useInitDesc()
    {
        return !isMoved && propType(&initDesc) != TypeNil;
    }

    /*
     *   Determine if I'm to be listed at all in my room description.
     *   Most objects should be listed normally, but some types of objects
     *   should be suppressed from the normal listing.  For example,
     *   fixed-in-place scenery objects are generally described in the
     *   custom message for the containing room, so these are normally
     *   omitted from the listing of the room's contents.
     *   
     *   By default, we'll return true, unless we are using our initial
     *   description; we return nil if our initial description is to be
     *   used because the initial description overrides the default
     *   listing.
     *   
     *   Individual objects are free to override this as needed to control
     *   their listing status.  
     */
    isListed() { return !useInitDesc(); }

    /*
     *   The default long description, which is displayed in response to
     *   an explicit player request to examine the object.  We'll use a
     *   generic library message; most objects should override this to
     *   customize the object's desription.  
     */
    lDesc { libMessages.thingLDesc(self); }

    /*
     *   "Equivalence" flag.  If this flag is set, then all objects with
     *   the same immediate superclass will be considered interchangeable;
     *   such objects will be listed collectively in messages (so we would
     *   display "five coins" rather than "a coin, a coin, a coin, a coin,
     *   and a coin"), and will be treated as equivalent in resolving noun
     *   phrases to objects in user input.
     *   
     *   By default, this property is nil, since we want most objects to
     *   be treated as unique.  
     */
    isEquivalent = nil

    /*
     *   Listing equivalence test.  This returns true if we should be
     *   treated as equivalent to the given item in a list, which is to
     *   say that we should be combined with the given item and reported
     *   as one of several such items.
     *   
     *   'options' is the set of LIST_xxx listing options for the object.
     *   In some cases, equivalency might depend upon the listing mode;
     *   for example, in 'tall' and 'recursive' listing mode, an object
     *   with children to display should probably not be marked as
     *   equivalent, since it would have to show its contents list in the
     *   recursive listing.
     *   
     *   By default, this returns true if this object is of the same class
     *   or classes as the given object.  This should be overridden when
     *   an object's listing format will show some kind of additional
     *   state about the item; for example, if the item is shown as
     *   "(providing light)" in listings, but its light can be turned on
     *   and off, items should be grouped only when their on/off states
     *   are the same.  
     */
    isListEquivalent(obj, options)
    {
        /* 
         *   by default, list myself with any other object of the
         *   identical set of superclasses 
         */
        return getSuperclassList() == obj.getSuperclassList();
    }

    /*
     *   "List Group" object.  If this property is set, then this object
     *   is grouped with other objects in the same list group for listing
     *   purposes.  This should be an object of class ListGroup.
     *   
     *   By default, we set this to nil, which makes an object ungrouped
     *   in listings.  
     */
    listWith = nil

    /*
     *   The strength of the light the object is giving off, if indeed it
     *   is giving off light.  This value should be one of the following:
     *   
     *   0: The object is giving off no light at all.
     *   
     *   1: The object is self-illuminating, but doesn't give off enough
     *   light to illuminate any other objects.  This is suitable for
     *   something like an LED digital clock.
     *   
     *   2: The object gives off dim light.  This level is bright enough
     *   to illuminate nearby objects, but not enough to reach distant
     *   objects or go through obscuring media, and not enough for certain
     *   activities requiring strong lighting, such as reading.
     *   
     *   3: The object gives off medium light.  This level is bright
     *   enough to illuminate nearby objects, and is enough for most
     *   activities, including reading and the like.  Traveling a distance
     *   or through an obscuring medium reduces this level to dim (2).
     *   
     *   4: The object gives off strong light.  This level is bright
     *   enough to illuminate nearby objects, and travel through an
     *   obscuring medium or over a distance reduces it to medium light
     *   (3).
     *   
     *   Note that the special value -1 is reserved as an invalid level,
     *   used to flag certain events (such as the need to recalculate the
     *   ambient light level from a new point of view).
     *   
     *   Most objects do not give off light at all.  
     */
    brightness = 0
    
    /*
     *   Sense sizes of the object.  Each object has an individual size
     *   for each sense.  By default, objects are medium for all senses;
     *   this allows them to be sensed from a distance or through an obscuring
     *   medium, but doesn't allow their details to be sensed.
     */
    sightSize = medium
    soundSize = medium
    smellSize = medium
    touchSize = medium

    /*
     *   Determine whether or not the object has a "presence" in each
     *   sense.  An object has a presence in a sense if an actor
     *   immediately adjacent to the object could detect the object by the
     *   sense alone.  For example, an object has a "hearing presence" if
     *   it is making some kind of noise, and does not if it is silent.
     *   
     *   Presence in a given sense is an intrinsic (which does not imply
     *   unchanging) property of the object, in that presence is
     *   independent of the relationship to any given actor.  If an alarm
     *   clock is ringing, it has a hearing presence, unconditionally; it
     *   doesn't matter if the alarm clock is sealed inside a sound-proof
     *   box, because whether or not a given actor has a sense path to the
     *   object is a matter for a different computation.
     *   
     *   Note that presence doesn't control access: an actor might have
     *   access to an object for a sense even if the object has no
     *   presence in the sense.  Presence indicates whether or not the
     *   object is actively emitting sensory data that would make an actor
     *   aware of the object without specifically trying to apply the
     *   sense to the object.
     *   
     *   By default, an object is visible and touchable, but does not emit
     *   any sound or odor.  
     */
    sightPresence = true
    soundPresence = nil
    smellPresence = nil
    touchPresence = true

    /*
     *   My "contents lister."  This is a ShowListInterface object that we
     *   use to display the contents of this object for room descriptions,
     *   inventories, and the like. 
     */
    contentsLister = thingContentsLister

    /*
     *   Determine if I can be sensed under the given conditions.  Returns
     *   true if the object can be sensed, nil if not.  If this method
     *   returns nil, this object will not be considered in scope for the
     *   current conditions.
     *   
     *   By default, we return nil if the ambient energy level for the
     *   object is zero.  If the ambient level is non-zero, we'll return
     *   true in 'transparent' conditions, nil for 'opaque', and we'll let
     *   the sense decide via its canSenseObj() method for any other
     *   transparency conditions.  
     */
    canBeSensed(sense, trans, ambient)
    {
        /* if the ambient level is zero, I can't be sensed this way */
        if (ambient == 0)
            return nil;
        
        /* check the viewing conditions */
        switch(trans)
        {
        case transparent:
            /* under transparent conditions, I appear as myself */
            return true;

        case obscured:
        case distant:
            /* 
             *   ask the sense to determine if I can be sensed under these
             *   conditions 
             */
            return sense.canSenseObj(self, trans, ambient);

        default:
            /* for any other conditions, I can't be sensed at all */
            return nil;
        }
    }

    /*
     *   Call a method on this object from the given point of view.  We'll
     *   push the current point of view, call the method, then restore the
     *   enclosing point of view. 
     */
    fromPOV(pov, propToCall, [args])
    {
        /* push the new point of view */
        pushPOV(pov);

        /* make sure we pop the point of view no matter how we leave */
        try
        {
            /* call the method */
            self.(propToCall)(args...);
        }
        finally
        {
            /* restore the enclosing point of view on the way out */
            popPOV();
        }
    }

    /*
     *   Every Thing has a location, which is the Thing that contains this
     *   object.  A Thing's location can only be a simple object
     *   reference, or nil; it cannot be a list, and it cannot be a method.
     *
     *   If the location is nil, the object does not exist anywhere in the
     *   simulation's physical model.  A nil location can be used to
     *   remove an object from the game world, temporarily or permanently.
     *
     *   In general, the 'location' property should be declared for each
     *   statically defined object (explicitly or implicitly via the '+'
     *   syntax).  'location' is a private property - it should never be
     *   evaluated or changed by any subclass or by any other object.
     *   Only Thing methods may evaluate or change the 'location'
     *   property.  So, you can declare a 'location' property when
     *   defining an object, but you should essentially never refer to
     *   'location' directly in any other context; instead, use the
     *   location and containment methods (isIn, etc) when you want to
     *   know an object's location.
     */
    location = nil

    /*
     *   Initialize my location's contents list - add myself to my
     *   container during initialization
     */
    initializeLocation()
    {
        if (location != nil)
            location.addToContents(self);
    }

    /*
     *   My contents.  This is a list of the objects that this object
     *   directly contains.
     */
    contents = []

    /*
     *   Show the contents of this object.  If the object has any
     *   contents, we'll display a listing of the contents.  
     *   
     *   'options' is the set of flags that we'll pass to showList(), and
     *   has the same meaning as for that function.
     *   
     *   'infoList' is a list of SenseInfoListEntry objects for the
     *   objects that the actor to whom we're showing the contents listing
     *   can see via the sight-like senses.
     *   
     *   This method should be overridden by any object that doesn't store
     *   its contents using a simple 'contents' list property.  
     */
    showObjectContents(pov, lister, options, indent, infoList)
    {
        local cont;

        /* add in the CONTENTS flag to the options */
        options |= LIST_CONTENTS;

        /* start with my direct contents */
        cont = contents;
        
        /* 
         *   keep only the visible objects - only objects in the infoList
         *   are visible from the listing actor's point of view 
         */
        cont = cont.subset({x: infoList.indexWhich({y: y.obj == x}) != nil});

        /* if the surviving list isn't empty, show it */
        if (cont != [])
            showList(pov, self, lister, cont, options, indent, infoList);
    }

    /*
     *   Determine if I'm is inside another Thing.  Returns true if this
     *   object is contained within obj.
     */
    isIn(obj)
    {
        /* if obj is my immediate container, I'm obviously in it */
        if (location == obj)
            return true;

        /* if I have no location, I'm obviously not in obj */
        if (location == nil)
            return nil;

        /* I'm in obj if my container is in obj */
        return location.isIn(obj);
    }

    /*
     *   Determine if I'm directly inside another Thing.  Returns true if
     *   this object is contained directly within obj.  Returns nil if
     *   this object isn't directly within obj, even if it is indirectly
     *   in obj (i.e., its container is directly or indirectly in obj).  
     */
    isDirectlyIn(obj)
    {
        /* I'm directly in obj only if it's my immediate container */
        return location == obj;
    }

    /*
     *   Add an object to my contents.
     */
    addToContents(obj)
    {
        /* add the object to my contents list */
        contents += obj;
    }

    /*
     *   Remove an object from my contents.
     */
    removeFromContents(obj)
    {
        /* remove the object from my contents list */
        contents -= obj;
    }

    /*
     *   Move this object to a new container.
     */
    moveInto(newContainer)
    {
        /* if I have a container, remove myself from its contents list */
        if (location != nil)
            location.removeFromContents(self);

        /* remember my new location */
        location = newContainer;

        /* note that I've been moved */
        isMoved = true;

        /*
         *   if I'm not being moved into nil, add myself to the
         *   container's contents
         */
        if (location != nil)
            location.addToContents(self);
    }

    /*
     *   Get the visual sense information for this object from the current
     *   global point of view.  If we have explicit sense information set
     *   with setSenseInfo, we'll return that; otherwise, we'll calculate
     *   the current sense information for the given point of view.
     *   Returns a SenseInfoListEntry object giving the information.  
     */
    getVisualSenseInfo()
    {
        local lst;
        
        /* if we have explicit sense information already set, use it */
        if (explicitVisualSenseInfo != nil)
            return explicitVisualSenseInfo;

        /* calculate the sense information for the point of view */
        lst = getPOV().visibleInfoList();

        /* find and return the information for myself */
        return lst.valWhich({x: x.obj == self});
    }

    /*
     *   Call a description method with explicit point-of-view and the
     *   related point-of-view sense information.  'pov' is the point of
     *   view object, which is usually an actor; 'senseInfo' is a
     *   SenseInfoListEntry object giving the sense information for this
     *   object, which getSenseInfo() will use instead of dynamically
     *   calculating the sense information for the duration of the routine
     *   called.  
     */
    withVisualSenseInfo(pov, senseInfo, methodToCall, [args])
    {
        local oldSenseInfo;
        
        /* push the sense information */
        oldSenseInfo = setVisualSenseInfo(senseInfo);

        /* push the point of view */
        pushPOV(pov);

        /* make sure we restore the old value no matter how we leave */
        try
        {
            /* call the method with the given arguments */
            self.(methodToCall)(args...);
        }
        finally
        {
            /* restore the old point of view */
            popPOV();
            
            /* restore the old sense information */
            setVisualSenseInfo(oldSenseInfo);
        }
    }

    /* 
     *   Set the explicit visual sense information; if this is not nil,
     *   getVisualSenseInfo() will return this rather than calculating the
     *   live value.  Returns the old value, which is a SenseInfoListEntry
     *   or nil.  
     */
    setVisualSenseInfo(info)
    {
        local oldInfo;

        /* remember the old value */
        oldInfo = explicitVisualSenseInfo;

        /* remember the new value */
        explicitVisualSenseInfo = info;

        /* return the original value */
        return oldInfo;
    }

    /* current explicit visual sense information overriding live value */
    explicitVisualSenseInfo = nil

    /*
     *   Determine how accessible my contents are to a sense.  Any items
     *   contained within a Thing are considered external features of the
     *   Thing, hence they are transparently accessible to all senses.
     */
    transSensingIn(sense) { return transparent; }

    /*
     *   Determine how accessible peers of this object are to the contents
     *   of this object, via a given sense.  This has the same meaning as
     *   transSensingIn(), but in the opposite direction: whereas
     *   transSensingIn() determines how accessible my contents are from
     *   the outside, this determines how accessible the outside is from
     *   the contents.
     *
     *   By default, we simply return the same thing as transSensingIn(),
     *   since most containers are symmetrical for sense passing from
     *   inside to outside or outside to inside.  However, we distinguish
     *   this as a separate method so that asymmetrical containers can
     *   have different effects in the different directions; for example,
     *   a box made of one-way mirrors might be transparent when looking
     *   from the inside to the outside, but opaque in the other
     *   direction.
     */
    transSensingOut(sense) { return transSensingIn(sense); }

    /*
     *   Determine how well I can sense the given object.  Returns a
     *   transparency level indicating how the sense passes from me to the
     *   given object.
     *   
     *   Returns a list consisting of the transparency level, the
     *   obstructor object, and the ambient sense energy level.  If the
     *   path's transparency level is 'transparent' or 'opaque', the
     *   obstructor will always be nil; if the level is 'distant' or
     *   'obscured', the obstructor will be the first object in the path
     *   that reduces the level.
     *   
     *   Note that, because 'distant' and 'obscured' transparency levels
     *   always compound (with one another and with themselves) to opaque,
     *   there will never be more than a single obstructor in a path,
     *   because any path with two or more obstructors would be an opaque
     *   path, and hence not a path at all.  
     */
    senseObj(sense, obj)
    {
        local bestTrans;
        local bestObstructor;
        local bestAmbient;

        /* we haven't found any path yet */
        bestTrans = opaque;
        bestObstructor = nil;
        bestAmbient = nil;

        /* 
         *   Iterate over everything reachable through the sense.  Keep
         *   track of the best path we find, and stop entirely if we find
         *   a transparent path. 
         */
        senseIter(sense, new function(cur, trans, obstructor, ambient) {
            /*
             *   If this is the object we're looking for, and this is the
             *   best transparency so far, note the transparency to this
             *   point.  
             */
            if (cur == obj && transparencyCompare(trans, bestTrans) > 0)
            {
                /* this is the best one yet - note it */
                bestTrans = trans;
                bestObstructor = obstructor;
                bestAmbient = ambient;

                /* 
                 *   if this path is completely transparent, we will never
                 *   find anything better, so return nil to tell the
                 *   caller to stop iterating 
                 */
                if (trans == transparent)
                    return nil;
            }

            /* tell the caller to keep searching */
            return true;
        });

        /* return the best transparency we found */
        return [bestTrans, bestObstructor, bestAmbient];
    }

    /*
     *   Determine this object's level of illumination for the given
     *   senses.  This returns the highest level of brightness of any
     *   energy source that this object can sense with the given senses,
     *   adjusted for transparency along the path to the energy source.
     *   
     *   This returns a brightness level with the same meaning as the
     *   'brightness' property.  
     */
    illuminationLevel(senses)
    {
        local illum;

        /* we have no illumination so far */
        illum = 0;
        
        /* check each of the requested senses */
        foreach (local sense in senses)
        {
            local curIllum;
            
            /* get the ambient energy level for this sense */
            curIllum = senseAmbient(sense);

            /* if this is the highest so far, note it */
            if (curIllum != nil && curIllum > illum)
                illum = curIllum;
        }

        /* return the highest illumination level we found */
        return illum;
    }

    /*
     *   Build a list of all of the objects reachable from me through the
     *   given sense.  
     */
    senseList(sense)
    {
        local lst;

        /* 
         *   start out with an empty vector (use a vector for efficiency -
         *   since we'll be continually adding objects, a vector is a lot
         *   more efficient than a list since a vector can expand without
         *   being reallocated) 
         */
        lst = new Vector(32);
        
        /*
         *   Iterate over everything reachable through the sense, and add
         *   each reachable object to the list 
         */
        senseIter(sense, new function(cur, trans, obstructor, ambient) {
            
            /* add the object to the list so far */
            lst += cur;

            /* tell the caller to proceed with the iteration */
            return true;
        });

        /* return the list we built (as an ordinary list) */
        return lst.toList();
    }

    /*
     *   Build a list of full information on all of the objects reachable
     *   from me through the given sense, along with full information for
     *   each object's sense characteristics.  For each object, the
     *   returned list will contain a SenseInfoList entry describing the
     *   sense conditions from the point of view of 'self' to the object.  
     */
    senseInfoList(sense)
    {
        local lst;

        /* start out with an empty vector */
        lst = new Vector(64);
        
        /*
         *   Iterate over everything reachable through the sense, and add
         *   each reachable object to the list 
         */
        senseIter(sense, new function(cur, trans, obstructor, ambient) {
            
            /* add the information to the list */
            lst += new SenseInfoListEntry(cur, trans, obstructor, ambient);

            /* tell the caller to proceed with the iteration */
            return true;
        });

        /* return the list we built (as an ordinary list) */
        return lst.toList();
    }

    /*
     *   Find an item in a senseInfoList list.  Returns the
     *   SenseInfoListEntry which describes the given item, or nil if
     *   there is no such entry.  
     */
    findSenseInfo(lst, item)
    {
        /* scan the items in the list */
        return lst.valWhich({x: x.obj == item});
    }

    /*
     *   Merge two senseInfoList lists.  Returns a new list containing
     *   only one instance of each item.  If the same object appears in
     *   more than one of the lists, the result list will have the
     *   occurrence with better detail or brightness.  
     */
    mergeSenseInfoList(a, b)
    {
        local lst;

        /* if either list is empty, just return the other list */
        if (a == [])
            return b;
        else if (b == [])
            return a;
        
        /* create a vector for the result list */
        lst = new Vector(max(a.length(), b.length()));
        
        /* 
         *   loop over the items in the first list - keep each item in the
         *   first list, but if it also appears in the second list then
         *   merge it to keep the better of the two occurrences 
         */
        foreach (local aEle in a)
        {
            local keepA;
            local bEle;

            /* assume we'll keep the occurrence from the first list */
            keepA = true;
            
            /* try finding this same item in the second list */
            if ((bEle = findSenseInfo(b, aEle)) != nil)
            {
                /* 
                 *   found it - keep the one with better transparency, or
                 *   better ambient sense energy if the transparencies are
                 *   the same 
                 */
                if (aEle.trans == bEle.trans)
                {
                    /* same transparency - compare energy levels */
                    if (aEle.ambient < bEle.ambient)
                        keepA = nil;
                }
                else if (transparencyCompare(aEle.trans, bEle.trans) < 0)
                {
                    /* a is less transparent than b */
                    keepA = nil;
                }
            }

            /* keep the item from the appropriate list */
            if (keepA)
            {
                /* keep the item from the first list */
                lst += aEle;
            }
            else
            {
                /* keep the item from the second list */
                lst += bEle;
            }
        }

        /* 
         *   loop over the second list, keeping only the items that aren't
         *   in the first list (any item that's also in the first list
         *   will already have been merged from the first pass) 
         */
        foreach (local bEle in b)
        {
            /* 
             *   find the item in the first list - if it's not in the
             *   first list, we need to add this item to the result list 
             */
            if (findSenseInfo(a, bEle) == nil)
            {
                /* keep this item */
                lst += bEle;
            }
        }

        /* return the merged result in list format */
        return lst.toList();
    }

    /*
     *   Iterate over all objects reachable via the given sense.  For each
     *   object, invokes the callback function.
     *   
     *   The callback returns true to continue the iteration, false to
     *   terminate it.  
     */
    senseIter(sense, func)
    {
        local ambient;
        
        /* 
         *   compute the ambient sense energy level (such as the light
         *   level) at the starting point 
         */
        ambient = senseAmbient(sense);

        /* 
         *   perform the iteration, starting with an empty path, an
         *   initial path transparency level of 'transparent', and no
         *   obstructor so far 
         */
        senseIterPath(sense, func, [], transparent, nil, ambient);
    }

    /*
     *   Determine the ambient sense energy level for the given sense from
     *   my point of view.  This is useful for senses such as sight, which
     *   can sense objects not only by the light they emit but also by the
     *   light they reflect.  For sight, this returns the ambient energy
     *   level at me, taking into account all light sources visible from
     *   my point of view.  
     */
    senseAmbient(sense)
    {
        local best;
        
        /* if the sense doesn't use ambient energy, we can ignore this */
        if (sense.ambienceProp == nil)
            return nil;

        /* we haven't seen any energy yet */
        best = 0;

        /* 
         *   Iterate over all objects in line of sight from me, looking
         *   for energy sources.  This is the correct direction to look,
         *   because we want to know if we can see an energy source - if
         *   we can, its energy reaches us and we are illuminated by it.  
         */
        senseIterPath(sense, new function(cur, trans, obstructor, ambient) {

            local br;
                
            /* 
             *   if this object is a energy source, and the amount of
             *   energy that reaches us is greater than the brightest
             *   energy we've seen so far, note it as the strongest energy
             *   level reaching us 
             */
            br = adjustBrightness(cur.(sense.ambienceProp), trans);
            if (br > best)
                best = br;

            /* tell the caller to continue */
            return true;
            
        }, [], transparent, nil, nil);

        /* 
         *   if the ambient level is level 1 (self-illuminating only),
         *   reduce it to nothing, since this doesn't cast energy on any
         *   of our surroundings and hence doesn't count as ambient energy 
         */
        if (best == 1)
            best = 0;

        /* return the best brightness level we found */
        return best;
    }

    /*
     *   Invoke the callback function for a sense iteration, passing self
     *   as the object.  We'll call this for each object we visit in the
     *   course of the iteration.  
     */
    senseIterCallFunc(func, sense, trans, obstructor, ambient)
    {
        /* 
         *   make some ambient energy adjustments if we have an ambient
         *   level at all (some senses do, some senses don't) 
         */
        if (ambient != nil)
        {
            local selfIllum;
            
            /* 
             *   if this object's own self-illumination in this sense is
             *   greater than the ambient level, use the object's
             *   self-illumination level instead 
             */
            selfIllum = self.(sense.ambienceProp);
            if (selfIllum > ambient)
                ambient = selfIllum;

            /* adjust the energy level for the transparency */
            ambient = adjustBrightness(ambient, trans);

            /* 
             *   if this leaves us with no energy at all, we can't sense
             *   the object after all 
             */
            if (ambient == 0)
                return true;
        }

        /* 
         *   Check to see if we can be sensed at all under the current
         *   conditions.  If we can, invoke the callback on this object.
         *   If we can't be sensed at all under these conditions, there's
         *   nothing more to do.  
         */
        if (canBeSensed(sense, trans, ambient))
        {
            /* I can be sensed under these conditions - call the callback */
            return func(self, trans, obstructor, ambient);
        }
        else
        {
            /* 
             *   I can't be sensed under these conditions - skip the
             *   callback and simply tell the caller to keep going 
             */
            return true;
        }
    }

    /*
     *   Iterate over objects that can be reached via the sense from this
     *   vantage.
     *   
     *   'path' is a list of objects that we have already visited and thus
     *   must exclude from further searching.
     *   
     *   'transToHere' is the transparency level up to this point.  If
     *   we're being called recursively, this is the cumulative
     *   transparency level along the path so far.  On the initial call,
     *   this should simply be 'transparent'.
     *   
     *   'obstructor' is the object along the path so far that introduced
     *   a non-transparent level.  On the initial call, this should be nil.
     *   
     *   'ambient' is the ambient sense level along the path so far.  Each
     *   time we traverse a transparent connection, we use the fact that
     *   the ambient level is the same on both sides of a transparent
     *   connection to avoid re-computing the ambient light level on the
     *   other side.  On the initial call, this should be the ambient
     *   light level from self's point of view, as computed with
     *   senseAmbient().  
     */
    senseIterPath(sense, func, path, transToHere, obstructor, ambient)
    {
        /* invoke the callback on myself */
        if (!senseIterCallFunc(func, sense, transToHere, obstructor, ambient))
            return nil;

        /* iterate up the containment hierarchy into my location */
        if (!senseIterUp(sense, func, path, transToHere, obstructor, ambient))
            return nil;

        /* sense down the hierarchy into my contents */
        return senseIterDown(sense, func, path,
                             transToHere, obstructor, ambient);
    }

    /*
     *   Iterate over objects reachable up the hierarchy from here.
     *   
     *   Note that we do not invoke the function on self - the caller is
     *   responsible for doing this.
     *   
     *   This is a single-parent implementation.  Multi-parent objects
     *   must override this method.  
     */
    senseIterUp(sense, func, path, transToHere, obstructor, ambient)
    {
        /* 
         *   if I have no location, there's no way up from here - simply
         *   return true in this case, so the caller knows to proceed with
         *   further iteration 
         */
        if (location == nil)
            return true;

        /* 
         *   if my container is already in the path, don't consider it,
         *   since we'd be getting into an infinite loop by going back to
         *   a location we've already considered 
         */
        if (path.indexOf(location) != nil)
            return true;

        /* add myself to the path, so that we don't loop back into me */
        path += self;

        /* look up into the container */
        return location.
            senseIterFromBelow(sense, func, path,
                               transToHere, obstructor, ambient);
    }

    /*
     *   Iterate down the containment hierarchy into my contents, visiting
     *   all of the objects reachable through the sense.
     *   
     *   Note that we do not invoke the callback on self - the caller is
     *   responsible for doing this.  
     */
    senseIterDown(sense, func, path, transToHere, obstructor, ambient)
    {
        local myTrans;

        /* 
         *   figure my transparency looking into my contents, and note if
         *   I'm the first obstructor of the sense so far 
         */
        myTrans = transSensingIn(sense);
        if (myTrans != transparent && obstructor == nil)
            obstructor = self;

        /* 
         *   add the transparency so far to the transparency to my own
         *   contents to get the cumulative transparency from the starting
         *   point to my contents 
         */
        transToHere = transparencyAdd(transToHere, myTrans);
        
        /* 
         *   if I am opaque from the outside, we cannot iterate into my
         *   contents - in this case, just return true to tell the caller
         *   to proceed with its own further iteration 
         */
        if (transToHere == opaque)
            return true;

        /* 
         *   if we have an ambient level, check the transparency looking
         *   out from my contents (we look out rather than in, because in
         *   this case the sense is traveling from the outside to the
         *   inside, hence we want to know what it looks like from the
         *   point of view of the contents) 
         */
        if (ambient != nil)
        {
            /* 
             *   get the transparency looking from inside to outside - if
             *   it's not transparent, we must re-calculate the ambient
             *   sense level within our contents - do so for our first
             *   child only, since all of our children will be at the same
             *   ambient level 
             */
            if (transSensingOut(sense) != transparent
                && contents.length() != 0)
            {
                /* re-calculate the ambient level for our first child */
                ambient = contents[1].senseAmbient(sense);
            }
        }
        
        /* iterate through my contents */
        return senseIterList(sense, func, path, transToHere, obstructor,
                             ambient, contents, &senseIterFromAbove);
    }

    /*
     *   Iterate through a given list of objects related to us in some way
     *   (contents, for example, or multiple containers), invoking the
     *   callback on the objects in the list and the objects further
     *   related to them. 
     */
    senseIterList(sense, func, path, transToHere, obstructor, ambient,
                  objList, objMethod)
    {
        /* add myself to the path, so that we don't loop back into me */
        path += self;

        /* iterate over the list */
        foreach (local cur in objList)
        {
            /* 
             *   if this object is in the path already, don't consider it
             *   - this would get us into a loop, since we've been this
             *   way once before 
             */
            if (path.indexOf(cur) != nil)
                continue;

            /* proceed into this object */
            if (!cur.(objMethod)(sense, func, path,
                                 transToHere, obstructor, ambient))
                return nil;
        }

        /* 
         *   we didn't have anyone tell us to stop, so tell the caller to
         *   proceed 
         */
        return true;
    }

    /*
     *   Iterate over objects reachable from here, traversing into this
     *   object from below us in the containment hierarchy - in other
     *   words, from a child of this object (something contained within
     *   this object).
     *   
     *   When traversing the tree from below, we'll scan both up and down
     *   the hierarchy from here. 
     */
    senseIterFromBelow(sense, func, path, transToHere, obstructor, ambient)
    {
        local myTrans;

        /* 
         *   If the ambient level is -1, this is a special flag indicating
         *   that we need to recalculate the ambient level from our point
         *   of view.  Callers will set this value when traversing a
         *   connection that invalidates the ambient level and they are
         *   unable to calculate the new level themselves.  
         */
        if (ambient == -1)
            ambient = senseAmbient(sense);

        /*
         *   Iterate over other children of the same container.  Items in
         *   the same container are not affected by the transparency
         *   properties of the container itself, since the sense doesn't
         *   have to pass through the container to go from one child to
         *   another.  
         */
        if (!senseIterList(sense, func, path, transToHere, obstructor,
                           ambient, contents, &senseIterFromAbove))
            return nil;

        /*
         *   Regardless of whether we can sense through the container or
         *   not, we can certainly sense the container itself (its
         *   insides, anyway), so invoke the callback on the container
         *   without further loss of transparency 
         */
        if (!senseIterCallFunc(func, sense, transToHere, obstructor, ambient))
            return nil;

        /*
         *   Note the transparency going through my containment boundary,
         *   passing from inside to outside; if I'm the first
         *   non-transparent boundary on this path, note that I'm the
         *   obstructor 
         */
        myTrans = transSensingOut(sense);
        if (myTrans != transparent && obstructor == nil)
            obstructor = self;

        /* 
         *   Accumulate the transparency of my boundary into the total
         *   transparency level to this point.  If this makes the total
         *   path opaque, we cannot sense anything above us.  
         */
        transToHere = transparencyAdd(transToHere, myTrans);
        if (transToHere != opaque)
        {
            /*
             *   if we have an ambient level, check the transparency
             *   looking in to my contents (we look in rather than out,
             *   because in this case the sense is traveling from the
             *   inside to the outside, hence we want to know what it
             *   looks like from the point of view of the container) 
             */
            if (ambient != nil
                && transSensingIn(sense) != transparent)
            {
                /* re-calculate the ambient level here */
                ambient = senseAmbient(sense);
            }
            
            /* iterate up through the container */
            if (!senseIterUp(sense, func, path,
                             transToHere, obstructor, ambient))
                return nil;
        }

        /* we didn't find any reason to stop the caller from continuing */
        return true;
    }

    /*
     *   Iterate into this object, coming from above the object in the
     *   containment hierarchy - in other words, from a parent (a
     *   container) of this object.
     *   
     *   When traversing the tree from above, we'll scan only down from
     *   here.  An object that exists in multiple containers does not by
     *   default serve as a sense conduit across those containers, so the
     *   fact that it has other locations is irrelevant to the search.
     *   Connectors override this, because they do connect all of their
     *   locations as sense conduits and hence must look back up the tree
     *   as well as down when entered from above.  
     */
    senseIterFromAbove(sense, func, path, transToHere, obstructor, ambient)
    {
        /* invoke the callback on myself */
        if (!senseIterCallFunc(func, sense, transToHere, obstructor, ambient))
            return nil;
        
        /* iterate into my contents */
        return senseIterDown(sense, func, path,
                             transToHere, obstructor, ambient);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   MultiLoc: this class can be multiply inherited by any object that
 *   must exist in more than one place at a time.  To use this class, put
 *   it BEFORE Thing (or any subclass of Thing) in the object's superclass
 *   list, to ensure that we override the default containment
 *   implementation for the object.  
 */
class MultiLoc: object
    /*
     *   We can be in any number of locations.  Our location must be given
     *   as a list.
     */
    locationList = []

    /*
     *   Initialize my location's contents list - add myself to my
     *   container during initialization
     */
    initializeLocation()
    {
        /*
         *   Add myself to each of my container's contents lists
         */
        locationList.forEach({loc: loc.addToContents(self)});
    }

    /*
     *   Determine if I'm in a given object, directly or indirectly
     */
    isIn(obj)
    {
        /* first, check to see if I'm directly in the given object */
        if (isDirectlyIn(obj))
            return true;

        /*
         *   Look at each object in my location list.  For each location
         *   object, if the location is within the object, I'm within the
         *   object.
         */
        return locationList.indexWhich({loc: loc.isIn(obj)}) != nil;
    }

    /*
     *   Determine if I'm directly in the given object
     */
    isDirectlyIn(obj)
    {
        /*
         *   we're directly in the given object only if the object is in
         *   my list of immediate locations
         */
        return (locationList.indexOf(obj) != nil);
    }

    /*
     *   Note that we don't need to override any of the contents
     *   management methods, since we provide special handling for our
     *   location relationships, not for our contents relationships.
     */

    /*
     *   Move this object into a given single container.  Removes the
     *   object from all of its other containers.
     */
    moveInto(newContainer)
    {
        /* remove myself from all of my current contents */
        locationList.forEach({loc: loc.removeFromContents(self)});

        /* set my location list to include only the new location */
        locationList = [newContainer];

        /* note that I've been moved */
        isMoved = true;

        /* add myself to my new container's contents */
        newContainer.addToContents(self);
    }

    /*
     *   Add this object to a new location.
     */
    moveIntoAdd(newContainer)
    {
        /* note that I've been moved */
        isMoved = true;

        /* add the new container to my list of locations */
        locationList += newContainer;

        /* add myself to my new container's contents */
        newContainer.addToContents(self);
    }

    /*
     *   Remove myself from a given container, leaving myself in any other
     *   containers.
     */
    moveOutOf(cont)
    {
        /* if I'm not actually directly in this container, do nothing */
        if (!isDirectlyIn(cont))
            return;

        /* remove myself from this container's contents list */
        cont.removeFromContents(self);

        /* note that I've been moved */
        isMoved = true;

        /* remove this container from my location list */
        locationList -= cont;
    }

    /*
     *   Look up the containment hierarchy at my parents, trying to find a
     *   path to the given object.  
     */
    senseIterUp(sense, func, path, transToHere, obstructor, ambient)
    {
        /* iterate over all of my locations */
        return senseIterList(sense, func, path, transToHere, obstructor,
                             ambient, locationList, &senseIterFromBelow);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Multi-Location item with automatic initialization.  This is a
 *   subclass of MultiLoc for objects with computed initial locations.
 *   Each instance must provide one of the following:
 *   
 *   - Override initializeLocation() with a method that initializes the
 *   location list by calling moveIntoAdd() for each location.  If this
 *   method isn't overridden, the default implementation will initialize
 *   the location list using initialLocationClass and/or isInitiallyIn(),
 *   as described below.
 *   
 *   - Define the method isInitiallyIn(loc) to return true if the 'loc' is
 *   one of the object's initial containers, nil if not.  The default
 *   implementation of this method simply returns true, so if this isn't
 *   overridden, every object matching the initialLocationClass() will be
 *   part of the contents list.
 *   
 *   - Define the property initialLocationClass as a class object.  We
 *   will add each instance of the class that passes the isInitiallyIn()
 *   test to the location list.  If this is nil, we'll test every object
 *   instance with the isInitiallyIn() method.  
 */
class AutoMultiLoc: MultiLoc
    /* initialize the location */
    initializeLocation()
    {
        /* get the list of locations */
        locationList = buildLocationList();

        /* add ourselves into each of our containers */
        foreach (local loc in locationList)
            loc.addToContents(self);
    }

    /*
     *   build my list of locations, and return the list 
     */
    buildLocationList()
    {
        local lst;

        /* we have nothing in our list yet */
        lst = new Vector(16);

        /*
         *   if initialLocationClass is defined, loop over all objects of
         *   that class; otherwise, loop over all objects
         */
        if (initialLocationClass != nil)
        {
            /* loop over all instances of the given class */
            for (local obj = firstObj(initialLocationClass) ; obj != nil ;
                 obj = nextObj(obj, initialLocationClass))
            {
                /* if the object passes the test, include it */
                if (isInitiallyIn(obj))
                    lst += obj;
            }
        }
        else
        {
            /* loop over all objects */
            for (local obj = firstObj() ; obj != nil ; obj = nextObj(obj))
            {
                /* if the object passes the test, include it */
                if (isInitiallyIn(obj))
                    lst += obj;
            }
        }

        /* return the list of locations */
        return lst.toList();
    }

    /*
     *   Class of our locations.  If this is nil, we'll test every object
     *   in the entire game with our isInitiallyIn() method; otherwise,
     *   we'll test only objects of the given class.
     */
    initialLocationClass = nil

    /*
     *   Test an object for inclusion in our initial location list.  By
     *   default, we'll simply return true to include every object.  We
     *   return true by default so that an instance can merely specify a
     *   value for initialLocationClass in order to place this object in
     *   every instance of the given class.
     */
    isInitiallyIn(obj) { return true; }
;

/*
 *   Dynamic Multi Location Item.  This is almost exactly the same as a
 *   regular multi-location item with automatic initialization, but the
 *   library will re-initialize the location of these items, by calling
 *   the object's reInitializeLocation(), at the start of every turn.  
 */
class DynamicMultiLoc: AutoMultiLoc
    reInitializeLocation()
    {
        local newList;

        /* build the new location list */
        newList = buildLocationList();

        /*
         *   Update any containers that are not in the intersection of the
         *   two lists.  Note that we don't simply move ourselves out of
         *   the old list and into the new list, because the two lists
         *   could have common members; to avoid unnecessary work that
         *   might result from removing ourselves from a container and
         *   then adding ourselves right back in to the same container, we
         *   only notify containers when we're actually moving out or
         *   moving in. 
         */

        /* 
         *   For each item in the old list, if it's not in the new list,
         *   notify the old container that we're being removed.
         */
        foreach (local loc in locationList)
        {
            /* if it's not in the new list, remove me from the container */
            if (newList.indexOf(loc) == nil)
                loc.removeFromContents(self);
        }

        /* 
         *   for each item in the new list, if we weren't already in this
         *   location, add ourselves to the location 
         */
        foreach (local loc in newList)
        {
            /* if it's not in the old list, add me to the new container */
            if (!isDirectlyIn(loc) == nil)
                loc.addToContents(self);
        }
        
        /* make the new location list current */
        locationList = newList;
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Openable: a mix-in class that can be combined with an object's other
 *   superclasses to make the object respond to the verbs "open" and
 *   "close."  
 */
class Openable: object
;

/* ------------------------------------------------------------------------ */
/*
 *   Lockable: a mix-in class that can be combined with an object's other
 *   superclasses to make the object respond to the verbs "lock" and
 *   "unlock."  A Lockable requires no key.
 *   
 *   Note that Lockable and LockableWithKey are mutually exclusive - an
 *   object can inherit from only one of these classes.  
 */
class Lockable: object
;

/* ------------------------------------------------------------------------ */
/*
 *   LockableWithKey: a mix-in class that can be combined with an object's
 *   other superclasses to make the object respond to the verbs "lock" and
 *   "unlock," with a key as an indirect object.  A LockableWithKey cannot
 *   be locked or unlocked except with the keys listed in the keyList
 *   property.
 *   
 *   Note that Lockable and LockableWithKey are mutually exclusive - an
 *   object can inherit from only one of these classes.  
 */
class LockableWithKey: object
    /* the list of objects that can serve as keys for this object */
    keyList = []
;

/* ------------------------------------------------------------------------ */
/*
 *   Container: an object that can have other objects placed within it.  
 */
class Container: Thing
    /* 
     *   My current open/closed state.  By default, this state never
     *   changes, but is fixed in the object's definition; for example, a
     *   box without a lid would always be open, while a hollow glass cube
     *   would always be closed.  Our default state is open. 
     */
    isOpen = true

    /* the material that the container is made of */
    material = adventium

    /*
     *   Determine how a sense passes to my contents.  If I'm open, the
     *   sense passes through directly, since there's nothing in the way.
     *   If I'm closed, the sense must pass through my material.  
     */
    transSensingIn(sense)
    {
        if (isOpen)
        {
            /* I'm open, so the sense passes through without interference */
            return transparent;
        }
        else
        {
            /* I'm closed, so the sense must pass through my material */
            return material.senseThru(sense);
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   OpenableContainer: an object that can contain things, and which can
 *   be opened and closed.  
 */
class OpenableContainer: Openable, Container
;

/* ------------------------------------------------------------------------ */
/*
 *   LockableContainer: an object that can contain things, and that can be
 *   opened and closed as well as locked and unlocked.  
 */
class LockableContainer: Lockable, OpenableContainer
;

/* ------------------------------------------------------------------------ */
/*
 *   KeyedContainer: an openable container that can be locked and
 *   unlocked, but only with a specified key.  
 */
class KeyedContainer: LockableWithKey, OpenableContainer
;


/* ------------------------------------------------------------------------ */
/*
 *   Surface: an object that can have other objects placed on top of it.
 *   A surface is essentially the same as a regular container, but the
 *   contents of a surface behave as though they are on the surface's top
 *   rather than contained within the object.  
 */
class Surface: Thing
    /* my contents lister */
    contentsLister = surfaceContentsLister
;


/* ------------------------------------------------------------------------ */
/*
 *   Room: the basic class for game locations.  This is the smallest unit
 *   of movement; we do not distinguish among locations within a room,
 *   even if a Room represents a physically large location.  If it is
 *   necessary to distinguish among different locations in a large
 *   physical room, simply divide the physical room into sections and
 *   represent each section with a separate Room object.
 *   
 *   A Room is not necessarily indoors; it is simply a location where an
 *   actor can be located.  This peculiar usage of "room" to denote any
 *   atomic location, even outdoors, was adopted by the authors of the
 *   earliest adventure games, and has been the convention ever since.
 *   
 *   A room's contents are the objects contained directly within the room.
 *   These include fixed features of the room as well as loose items in
 *   the room, which are effectively "on the floor" in the room.  
 */
class Room: Thing
    /*
     *   Most rooms provide their own implicit lighting.  We'll use
     *   'medium' lighting (level 3) by default, which provides enough
     *   light for all activities, but is reduced to dim light (level 2)
     *   when it goes through obscuring media or over distance.  
     */
    brightness = 3

    /*
     *   Flag: we've seen this location before.  We'll set this to true
     *   the first time we see the location (as long as the location isn't
     *   dark). 
     */
    isSeen = nil

    /*
     *   The room's long description.  This is displayed in a "verbose"
     *   display of the room.  
     */
    lDesc = ""

    /* 
     *   The room's first description.  This is the description we'll
     *   display the first time we see the room when it isn't dark (it's
     *   the first time if isSeen is nil).  By default, we'll just display
     *   the normal long description, but a room can override this if it
     *   wants a special description on the first non-dark arrival.  
     */
    firstDesc { lDesc; }

    /*
     *   Display my short description when the room is dark.  By default,
     *   we display a standard library message. 
     */
    darkSDesc { libMessages.roomDarkSDesc; }

    /* 
     *   Display my long description when the room is dark.  Note that
     *   authors must take special care to list items that are both
     *   self-illuminating and marked as non-listed (isListed = nil) here,
     *   because such items will not be otherwise displayed (non-listed
     *   items are non-listed in a dark room, just like in a lit room).  
     */
    darkLDesc { libMessages.roomDarkLDesc; }

    /*
     *   Describe the room.  Produces the full description of the room if
     *   'verbose' is true, or a briefer description if not.
     *   
     *   Note that this method must be overridden if the room uses a
     *   non-conventional contents mechanism (i.e., it doesn't store its
     *   complete set of direct contents in a 'contents' list property).  
     */
    lookAround(actor, verbose)
    {
        local illum;
        local infoList;

        /* 
         *   get a list of all of the objects that the actor can sense in
         *   the location using sight-like senses (such as sight) 
         */
        infoList = actor.visibleInfoList();

        /* 
         *   drop the actor, to ensure that we don't list the actor doing
         *   the looking among the room's contents 
         */
        infoList = infoList.subset({x: x.obj != actor});
        
        /* 
         *   check for ambient illumination in the room for the actor's
         *   sight-like senses 
         */
        illum = illuminationLevel(actor.sightlikeSenses);
        
        /* display the short description */
        "<.roomname>";
        statusDescIllum(actor, illum);
        "<./roomname>";

        /* if we're in verbose mode, display the full description */
        if (verbose)
        {
            local initDescItems;

            "<.roomdesc>";
            
            /* 
             *   check for illumination - we must have at least dim
             *   ambient lighting (level 2) to see the room's long
             *   description 
             */
            if (illum > 1)
            {
                /* 
                 *   display the normal description of the room - use the
                 *   firstDesc if this is the first time in the room,
                 *   otherwise the lDesc 
                 */
                if (isSeen)
                {
                    /* we've seen it already - show the regular description */
                    lDesc;
                }
                else
                {
                    /* 
                     *   we've never seen this location before - show the
                     *   first-time description 
                     */
                    firstDesc;

                    /* note that we've seen it now */
                    isSeen = true;
                }
            }
            else
            {
                /* display the in-the-dark description of the room */
                darkLDesc;
            }

            "<./roomdesc>";

            /* 
             *   Display any initial-location messages for objects
             *   directly within the room.  These messages are part of the
             *   verbose description rather than the portable item
             *   listing, because these messages are meant to look like
             *   part of the room's full description and thus should not
             *   be included in a non-verbose listing.
             *   
             *   Start out with everything in the room's contents.  
             */
            initDescItems = contents;

            /* 
             *   If the room isn't at least dimly lit (level 2), only keep
             *   items that are self-illuminating.  
             */
            if (illum < 2)
            {
                /* 
                 *   keep only the items sensible to the actor - note that
                 *   it's faster to compute the entire list of sensible
                 *   objects and then intersect it with our contents list
                 *   than it is to test each item in the contents list
                 *   separately, because the list computation is optimized
                 *   to carry light levels throughout the calculation,
                 *   whereas testing each item individually requires
                 *   starting from scratch on each item 
                 */
                initDescItems = initDescItems.subset(
                    {x: infoList.indexWhich({y: y.obj == x}) != nil});
            }

            /* show each initial description item */
            foreach (local cur in initDescItems)
            {
                /* if this item uses an initial description, display it */
                if (cur.useInitDesc())
                {
                    /* start a new paragraph */
                    "<.roompara>";
                    
                    /* display the item's initial description */
                    cur.initDesc;
                }
            }
        }

        /* 
         *   Describe each visible object directly contained in the room
         *   that wants to be listed - generally, we list any mobile
         *   objects that don't have special initial descriptions.  We
         *   list items in all room descriptions, verbose or not, because
         *   the portable item list is more of a status display than it is
         *   a part of the full description.
         */
        showRoomContents(actor, illum, infoList);
    }

    /*
     *   Display the contents of the room.  The illumination level is
     *   provided so we know how to construct the list.
     *   
     *   Note that this method must be overridden if the room uses a
     *   non-conventional contents mechanism (i.e., it doesn't store its
     *   complete set of direct contents in a 'contents' list property).  
     */
    showRoomContents(actor, illum, infoList)
    {
        local lst;
        local lister;

        /* 
         *   if the illumination is less than 'dim' (level 2), display
         *   only self-illuminating items 
         */
        if (illum != nil && illum < 2)
        {
            /* 
             *   We're in the dark - list only those objects that the
             *   actor can sense via the sight-like senses, which will be
             *   the list of self-illuminating objects.  (To produce this
             *   list, simply make a list consisting of only the 'obj'
             *   from each sense info entry in the sense info list.)  
             */
            lst = infoList.mapAll({x: x.obj});
            
            /* use my dark lister */
            lister = darkRoomLister;
        }
        else
        {
            /* start with my contents list */
            lst = contents;

            /* always remove the actor from the list */
            lst -= actor;

            /* use the normal (lighted) lister */
            lister = roomLister;
        }

        /* start a new paragraph if the list isn't empty */
        if (lst != [])
            "<.roompara>";

        /* show the contents */
        showList(actor, self, lister, lst, 0, 0, infoList);

        /* show the (visible) contents of the contents */
        showContentsList(actor, lst, 0, 0, infoList);
    }

    /* 
     *   Get my lighted room lister - this is the ShowListInterface object
     *   that we use to display the room's contents when the room is lit.
     *   We'll return the default library room lister.
     */
    roomLister { return libMessages.roomLister; }

    /* 
     *   Get my dark room lister - this is the ShowListInterface object
     *   we'll use to display the room's self-illuminating contents when
     *   the room is dark. 
     */
    darkRoomLister { return libMessages.darkRoomLister; }

    /*
     *   Display the "status line" description of the room.  This is
     *   normally a brief, single-line description giving, effectively,
     *   the name of the room.
     *   
     *   By long-standing convention, each location in a game usually has
     *   a distinctive name that's displayed here.  Players usually find
     *   these names helpful in forming a mental map of the game.
     *   
     *   By default, we'll simply display the room's short description.
     */
    statusDesc(actor)
    {
        /* the description depends on whether we're illuminated or not */
        statusDescIllum(actor, illuminationLevel(actor.sightlikeSenses));
    }

    /* 
     *   Display the status line description given our illumination
     *   status.  (This separate version of statusDesc is provided so that
     *   we can avoid re-calculating our illumination status if the caller
     *   has already done so for its own purposes.) 
     */
    statusDescIllum(actor, illum)
    {
        /* 
         *   Check for illumination.  We must have at least dim light
         *   (level 2) to show the room's description. 
         */
        if (illum > 1)
        {
            /* there's light - display my short description */
            sDesc;
        }
        else
        {
            /* no light - display the in-the-dark message */
            darkSDesc;
        }
    }
;

/*
 *   A dark room, which provides no light of its own 
 */
class DarkRoom: Room
    /* 
     *   turn off the lights 
     */
    brightness = 0
;


/* ------------------------------------------------------------------------ */
/*
 *   Connector: an object that can pass senses across room boundaries.
 *   This is a mix-in class.
 *   
 *   A Connector acts as a sense conduit across all of its locations, so
 *   to establish a connection between locations, simply place a Connector
 *   in each location.  Since a Connector is useful only when placed
 *   placed in multiple locations, Connector is based on MultiLoc.  
 */
class Connector: MultiLoc
    /*
     *   A Connector's material generally determines how senses pass
     *   through the connection.
     */
    connectorMaterial = adventium

    /*
     *   Determine how senses pass through this connection.  By default,
     *   we simply use the material's transparency.
     */
    transSensingThru(sense) { return connectorMaterial.senseThru(sense); }

    /*
     *   A Connector is a sense conduit connecting all of the locations in
     *   which it exists.  Therefore, when traversing the containment tree
     *   looking for a sense path, we must always traverse all of a
     *   Connector's parents, even when we are coming into this object
     *   from one of our parents.  (The 'path' exclusion prevents us from
     *   traversing back into the same parent we just came from, of
     *   course.)  
     */
    senseIterFromAbove(sense, func, path, transToHere, obstructor, ambient)
    {
        local myTrans;
        local transUp;
        local obstructorUp = obstructor;

        /* 
         *   First, try looking up the containment hierarchy at my other
         *   locations.  This has the effect of looking from one of our
         *   containers, through our connection material, to each of our
         *   other containers.
         *   
         *   Since we're looking through our connection material, we must
         *   find the cumulative transparency of the full path through the
         *   material.
         *   
         *   Note that if I'm the first non-transparent item in the path
         *   so far, I'm the obstructor on this path.  
         */
        myTrans = transSensingThru(sense);
        if (myTrans != transparent && obstructor == nil)
            obstructorUp = self;

        /* note the accumulated transparency along the through path */
        transUp = transparencyAdd(myTrans, transToHere);

        /* 
         *   iterate up the hierarchy into my other containers - note that
         *   we need only do this if the total path so far is not opaque 
         */
        if (transUp != opaque)
        {
            local myAmbient = ambient;
            
            /* 
             *   if the connection is not transparent, we must
             *   re-calculate the ambient light level when get to each
             *   other connected location 
             */
            if (ambient != nil && myTrans != transparent)
            {
                /* 
                 *   Set the ambient level to -1 to indicate that we must
                 *   re-calculate the ambient level for each location.  We
                 *   can't calculate it ourselves, because each location
                 *   will need to calculate it separately. 
                 */
                myAmbient = -1;
            }
            
            /* iterate up through my other containers */
            if (!senseIterUp(sense, func, path,
                             transUp, obstructorUp, myAmbient))
                return nil;
        }

        /*
         *   We can sense this object itself.  Note that our connection
         *   material's transparency is not relevant here, since we do not
         *   have to look through ourself to see ourself! 
         */
        if (!senseIterCallFunc(func, sense, transToHere, obstructor, ambient))
            return nil;

        /* 
         *   Finally, try looking down the hierarchy at my children.  Note
         *   that, when looking at my children, we do not consider our
         *   connection material's transparency, because we do not see
         *   children through the connecting material.  
         */
        return senseIterDown(sense, func, path,
                             transToHere, obstructor, ambient);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   An item that can be worn
 */
class Wearable: Thing
    /* is the item currently being worn? */
    isWorn()
    {
        /* it's being worn if the wearer is non-nil */
        return wornBy != nil;
    }

    /* 
     *   The object wearing this object, if any; if I'm not being worn,
     *   this is nil.  The wearer should always be a container (direct or
     *   indirect) of this object - in order to wear something, you must
     *   be carrying it.  In most cases, the wearer should be the direct
     *   container of the object.
     *   
     *   The reason we keep track of who's wearing the object (rather than
     *   simply keeping track of whether it's being worn) is to allow for
     *   cases where an actor is carrying another actor.  Since this
     *   object will be (indirectly) inside both actors in such cases, we
     *   would have to inspect intermediate containers to determine
     *   whether or not the outer actor was wearing the object if we
     *   didn't keep track of the wearer directly.  
     */
    wornBy = nil

    /* am I worn by the given object? */
    isWornBy(actor)
    {
        return wornBy == actor;
    }

    /*
     *   Determine if I'm equivalent to another object of the same class
     *   for listing purposes.  If we're showing the "(being worn)"
     *   status, then to be equivalent, two wearables must have the same
     *   being-worn state to be listed together.  
     */
    isListEquivalent(obj, options)
    {
        /* 
         *   if we're not equivalent by the standard test, we're not
         *   equivalent 
         */
        if (!inherited.isListEquivalent(obj, options))
            return nil;

        /* 
         *   if we're displaying an explicit list of worn items, then we
         *   won't need a "being worn" status message, so this doesn't
         *   differentiate objects - in this case, the objects are
         *   equivalent regardless of their 'worn' status 
         */
        if (!(options & LIST_WORN))
            return true;
        
        /* 
         *   we pass the standard test, but we're showing the 'worn'
         *   status in the display - the objects are thus equivalent for
         *   listing purposes only if their states are the same 
         */
        return isWorn() == obj.isWorn();
    }
    
    /* show myself in a listing */
    showListItem(options, pov, senseInfo)
    {
        /* inherit the default handling */
        inherited.showListItem(options, pov, senseInfo);

        /* show our 'worn' status */
        wornDesc(options);
    }

    /* show myself in a counted listing */
    showListItemCounted(equivCount, options, pov, senseInfo)
    {
        /* inherit the default handling */
        inherited.showListItemCounted(equivCount, options, pov, senseInfo);

        /* show our 'worn' status */
        wornDesc(options);
    }

    /* show my 'worn' status */
    wornDesc(options)
    {
        /* 
         *   show our status only if we're being worn and this isn't an
         *   explicit list of items being worn (if it is, the list will
         *   announce that all of the items are being worn, hence the
         *   individual items don't need to be marked as such) 
         */
        if (isWorn() && !(options & LIST_WORN))
            libMessages.wornDesc;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   An item that can provide light.
 *   
 *   Any Thing can provide light, but this class should be used for
 *   objects that explicitly serve as light sources from the player's
 *   perspective.  Objects of this class display a "providing light"
 *   status message in inventory listings, and can be turned on and off
 *   via the isLit property.  
 */
class LightSource: Thing
    /* is the light source currently turned on? */
    isLit = true

    /* the brightness that the object has when it is on and off */
    brightnessOn = 3
    brightnessOff = 0

    /* 
     *   return the appropriate on/off brightness, depending on whether or
     *   not we're currently lit 
     */
    brightness { return isLit ? brightnessOn : brightnessOff; }

    /*
     *   Determine if I'm equivalent to another object of the same class
     *   for listing purposes.  To be equivalent, two light sources must
     *   have the same isLit status, because we show the status (via a
     *   "providing light" message) in listings.  
     */
    isListEquivalent(obj, options)
    {
        /* 
         *   if we're not equivalent by the standard test, we're not
         *   equivalent 
         */
        if (!inherited.isListEquivalent(obj, options))
            return nil;
        
        /* 
         *   we pass the standard test, so check to see if the 'lit'
         *   states of the two objects are the same - if they are, we're
         *   equivalent, otherwise we're not 
         */
        return isLit == obj.isLit;
    }

    /* show myself in a listing */
    showListItem(options, pov, senseInfo)
    {
        /* inherit the default handling */
        inherited.showListItem(options, pov, senseInfo);

        /* show our light status */
        providingLightDesc;
    }

    /* show myself in a counted listing */
    showListItemCounted(equivCount, options, pov, senseInfo)
    {
        /* inherit the default handling */
        inherited.showListItemCounted(equivCount, options, pov, senseInfo);

        /* show our light status */
        providingLightDesc;
    }

    /* 
     *   show our "providing light" status, but only if we actually are
     *   providing light 
     */
    providingLightDesc
    {
        /* 
         *   if we're providing light to surrounding objects, say so -
         *   note that we do not provide light to any nearby objects if
         *   we're merely self-illuminating (which is the case if our
         *   brightness is 1) 
         */
        if (brightness > 1)
            libMessages.providingLightDesc;
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   An Actor is a living person, animal, or other entity with a will of
 *   its own.
 *   
 *   An actor's contents are the things the actor is carrying or wearing.  
 */
class Actor: Thing
    /*
     *   Actors usually have proper names, so they don't need articles
     *   when their names are used (just "Bob", not "a Bob" or "the Bob") 
     */
    aDesc { sDesc; }
    theDesc { sDesc; }
    
    /*
     *   The senses that determine scope for this actor.  An actor might
     *   possess only a subset of the defined sense.
     *   
     *   By default, we give each actor all of the human senses that we
     *   define, except touch.  In general, merely being able to touch an
     *   object doesn't put the object in scope, because an actor can
     *   usually touch a number of objects that aren't immediately within
     *   reach.  
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
     *   Build a list of all of the objects of which an actor is aware.
     *   
     *   An actor is aware of an object if the object is within reach of
     *   the actor's senses, and has some sort of presence in that sense.
     *   Note that both of these conditions must be true for at least one
     *   sense possessed by the actor; an object that is within earshot,
     *   but not within reach of any other sense, is in scope only if the
     *   object is making some kind of noise.  
     */
    scopeList()
    {
        local lst;

        /* we have nothing in our master list yet */
        lst = [];

        /* iterate over each sense */
        foreach (local sense in scopeSenses)
        {
            /* 
             *   get everything reachable to this sense, and add the
             *   unique elements into the result list 
             */
            lst = lst.appendUnique(senseScopeList(sense));
        }

        /* return the result */
        return lst;
    }

    /*
     *   Build a list of all of the objects of which the actor is aware
     *   through the sight-like senses.  
     */
    visibleList()
    {
        local lst;

        /* we have nothing in our master list yet */
        lst = [];

        /* iterate over each sense */
        foreach (local sense in sightlikeSenses)
        {
            local cur;
            
            /* get information for all objects for the current sense */
            cur = senseList(sense);

            /* add the unique elements */
            lst = lst.appendUnique(cur);
        }

        /* return the result */
        return lst;
    }

    /*
     *   Build a list of full sight-like sensory information for all of
     *   the objects visible to the actor through the actor's sight-like
     *   senses.  Returns a list with the same set of information as
     *   senseInfoList().  
     */
    visibleInfoList()
    {
        local lst;

        /* we have nothing in our master list yet */
        lst = [];

        /* iterate over each sense */
        foreach (local sense in sightlikeSenses)
        {
            local cur;
            
            /* get information for all objects for the current sense */
            cur = senseInfoList(sense);

            /* merge the two lists */
            lst = mergeSenseInfoList(cur, lst);
        }

        /* return the result */
        return lst;
    }


    /*
     *   Build a list of all of the objects that are in scope for this
     *   actor through the given sense.  
     */
    senseScopeList(sense)
    {
        local lst;

        /* we don't have any objects in our list yet */
        lst = new Vector(32);
        
        /*
         *   Iterate over everything reachable through the sense, and add
         *   each reachable object to the list 
         */
        senseIter(sense, new function(cur, trans, obstructor, ambient) {
            /* 
             *   include the object only if it has a presence in this
             *   sense - if it doesn't, the actor is not made aware of the
             *   object by this sense, because even though it is within
             *   reach of the sense, the object would not be detectable by
             *   this sense alone no matter what its relation to the actor 
             */
            if (cur.(sense.presenceProp))
            {
                /* add the object to the list so far */
                lst += cur;
            }

            /* tell the caller to proceed with the iteration */
            return true;
        });

        /* return the list we built */
        return lst.toList();
    }

    /*
     *   Show what the actor is carrying.
     *   
     *   Note that this method must be overridden if the actor does not
     *   use a conventional 'contents' list property to store its full set
     *   of contents.  
     */
    showInventory(tall)
    {
        local infoList;

        /* 
         *   Get the list of objects visible to the actor through the
         *   actor's sight-like senses, along with the sense information
         *   for each object.  We'll only worry about this for the
         *   contents of the items being carried; we'll assume that the
         *   actor has at some point taken note of each item being
         *   directly carried (by virtue of having picked it up at some
         *   point in the past) and can still identify each by touch, even
         *   if it's too dark to see it.  
         */
        infoList = visibleInfoList();

        /* list in the appropriate mode ("wide" or "tall") */
        if (tall)
        {
            /* 
             *   show the list of items being carried, recursively
             *   displaying the contents; show items being worn in-line
             *   with the regular listing 
             */
            showList(self, self, inventoryLister, contents,
                     LIST_TALL | LIST_RECURSE | LIST_INVENT,
                     0, infoList);
        }
        else
        {
            local carrying;
            local wearing;

            /* 
             *   paragraph style ("wide") inventory - go through the
             *   direct contents, and separate the list into what's being
             *   carried and what's being worn 
             */
            carrying = new Vector(32);
            wearing = new Vector(32);
            foreach (local cur in contents)
            {
                if (cur.isWornBy(self))
                    wearing += cur;
                else
                    carrying += cur;
            }

            /* convert the carrying and wearing vectors to lists */
            carrying = carrying.toList();
            wearing = wearing.toList();
            
            /* show the list of items being carried */
            showList(self, self, inventoryLister, carrying, LIST_INVENT,
                     0, infoList);

            /* show the visible contents of the items being carried */
            showContentsList(self, carrying, LIST_INVENT, 0, infoList);
            
            /* show the list of items being worn */
            if (wearing.length() != 0)
            {
                /* show the items being worn */
                showList(self, self, wearingLister, wearing, LIST_WORN, 0,
                         infoList);

                /* show the contents of the items being worn */
                showContentsList(self, wearing, LIST_WORN, 0, infoList);
            }
        }
    }

    /*
     *   The ShowListInterface object that we use for inventory listings 
     */
    inventoryLister
    {
        /* 
         *   if I'm the player character, use the player character lister;
         *   otherwise use the non-player lister 
         */
        if (libGlobal.playerChar == self)
            return libMessages.playerInventoryLister;
        else
            return self.myInventoryLister;
    }

    /*
     *   The ShowListInterface object that we use for inventory listings
     *   of items being worn 
     */
    wearingLister
    {
        if (libGlobal.playerChar == self)
            return libMessages.playerWearingLister;
        else
            return self.myWearingLister;
    }

    /* 
     *   My inventory lister item.  During pre-initialization, the library
     *   will automatically create an instance of NonPlayerInventoryLister
     *   for each actor that doesn't define a non-nil value for this
     *   property.  Note that we never use this for the current player
     *   character, but rather use libMessages.playerInventoryLister.  
     */
    myInventoryLister = nil

    /* my inventory lister for items being worn */
    myWearingLister = nil

    /*
     *   Perform library pre-initialization on the actor 
     */
    initializeActor() { }
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
    setRootPOV(actor);
}

