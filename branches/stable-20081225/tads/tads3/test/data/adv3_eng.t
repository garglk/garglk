/* Copyright 2000, 2002 Michael J. Roberts */
/*
 *   TADS 3 library - English language module
 *   
 *   The library attempts to isolate everything related to producing
 *   messages in English to this module.  When translating the library to
 *   a non-English language, this module must be replaced with an
 *   appropriate implementation for the alternative language.  It is the
 *   intention of the library's designers that the library can be
 *   translated by replacing this module, without changing any of the
 *   other portions of the library.  
 */

#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Language-specific globals 
 */
languageGlobals: object
    /*
     *   The character to use to separate groups of digits in large
     *   numbers.  US English uses commas; most Europeans use periods.
     *   
     *   Note that this setting does not affect system-level BigNumber
     *   formatting, but this information can be passed when calling
     *   BigNumber formatting routines.  
     */
    digitGroupSeparator = ','

    /*
     *   The decimal point to display in floating-point numbers.  US
     *   English uses a period; most Europeans use a comma.
     *   
     *   Note that this setting doesn't affect system-level BigNumber
     *   formatting, but this information can be passed when calling
     *   BigNumber formatting routines.  
     */
    decimalPointCharacter = '.'
;


/* ------------------------------------------------------------------------ */
/*
 *   Language-specific base class for Thing.  This class contains the
 *   methods and properties of Thing that need to be replaced when the
 *   library is translated to another language.
 *   
 *   The properties and methods defined here should generally never be
 *   used by language-independent library code, because everything defined
 *   here is specific to English.  Translators are thus free to change the
 *   entire scheme defined here.  
 */
class LangThing: object
    /*
     *   Flag that this object's name is rendered as a plural (this
     *   applies to both a singular noun with plural usage, such as
     *   "pants," and an object used to represent a collection of physical
     *   objects, such as "shrubs").  
     */
    isPlural = nil

    /*
     *   The short description string.  Each instance should override this
     *   to define the name of the object.  This string should not contain
     *   any articles; we use this string as the root to generate various
     *   forms of the object's name for use in different places in
     *   sentences.  
     */
    sDescStr = ''

    /*
     *   Display the short description.  By default, we'll simply display
     *   the short-description-string property, sDescStr. 
     */
    sDesc { "<<sDescStr>>"; }

    /*
     *   Display a count of the object ("five coins").  'info' is a
     *   SenseInfoListEntry giving the sense conditions under which the
     *   object is being described.  
     */
    countDesc(count)
    {
        /* 
         *   display the number followed by a space - spell out numbers
         *   below 100, but use numerals to denote larger numbers 
         */
        "<<spellIntBelowExt(count, 100, 0, INTDEC_GROUP_SEP)>> ";

        /* add the plural name */
        pluralDesc;
    }

    /* display a pronoun for the object in the nominitive/object case */
    itDescNom { if (isPlural) "they"; else "it"; }
    itDescObj { if (isPlural) "them"; else "it"; }

    /* display the name with a definite article ("the box") */
    theDesc { "the <<sDescStr>>"; }

    /* display my name and a being verb ("the box is") */
    nameIsDesc { "<<theDesc>> <<isPlural ? 'are' : 'is'>>"; }

    /* 
     *   the indefinite article to use; if this is nil, we'll try to
     *   figure it out by looking at the sDescStr 
     */
    articleIndef = nil

    /* 
     *   Display the name with an indefinite article ("a box").  If an
     *   'articleIndef' property is defined to non-nil, we'll display that
     *   article; otherwise, we'll try to figure it out by looking at the
     *   sDescStr property.
     *   
     *   By default, we'll use the article "a" if the sDescStr starts with
     *   a consonant, or "an" if it starts with a vowel.
     *   
     *   If the sDescStr starts with a "y", we'll look at the second
     *   letter; if it's a consonant, we'll use "an", otherwise "a" (hence
     *   "an yttrium block" but "a yellow brick").
     *   
     *   If the object is marked as having plural usage, we will use
     *   "some" as the article ("some pants" or "some shrubs").
     *   
     *   Some objects will want to override the default behavior, because
     *   the lexical rules about when to use "a" and "an" are not without
     *   exception.  For example, silent-"h" words ("honor") are written
     *   with "an", and "h" words with a pronounced but weakly stressed
     *   initial "h" are sometimes used with "an" ("an historian").  Also,
     *   some 'y' words might not follow the generic 'y' rule.
     *   
     *   'U' words are especially likely not to follow any lexical rule -
     *   any 'u' word that sounds like it starts with 'y' should use 'a'
     *   rather than 'an', but there's no good way to figure that out just
     *   looking at the spelling (consider "a universal symbol" and "an
     *   unimportant word", or "a unanimous decision" and "an unassuming
     *   man").  We simply always use 'an' for a word starting with 'u',
     *   but this will have to be overridden when the 'u' sounds like 'y'.
     *   
     *   Note that this routine can be overridden entirely, but in most
     *   cases, an object can simply override the 'article' property to
     *   specify the article to use.  
     */
    aDesc
    {
        /* 
         *   The complete list of unaccented, accented, and ligaturized
         *   Latin vowels from the Unicode character set.  (The Unicode
         *   database doesn't classify characters as vowels or the like,
         *   so it seems the only way we can come up with this list is
         *   simply to enumerate the vowels.)
         *   
         *   These are all lower-case letters; all of these are either
         *   exclusively lower-case or have upper-case equivalents that
         *   map to these lower-case letters.
         *   
         *   (Note an implementation detail: the compiler will append all
         *   of these strings together at compile time, so we don't have
         *   to perform all of this concatenation work each time we
         *   execute this method.)  
         */
        local vowels = 'aeiou\u00E0\u00E1\u00E2\u00E3\u00E4\u00E5\u00E6'
                       + '\u00E8\u00E9\u00EA\u00EB\u00EC\u00ED\u00EE\u00EF'
                       + '\u00F2\u00F3\u00F4\u00F5\u00F6\u00F8\u00F9\u00FA'
                       + '\u00FB\u00FC\u0101\u0103\u0105\u0113\u0115\u0117'
                       + '\u0119\u011B\u0129\u012B\u012D\u012F\u014D\u014F'
                       + '\u0151\u0169\u016B\u016D\u016F\u0171\u0173\u01A1'
                       + '\u01A3\u01B0\u01CE\u01D0\u01D2\u01D4\u01D6\u01D8'
                       + '\u01DA\u01DC\u01DF\u01E1\u01E3\u01EB\u01ED\u01FB'
                       + '\u01FD\u01FF\u0201\u0203\u0205\u0207\u0209\u020B'
                       + '\u020D\u020F\u0215\u0217\u0254\u025B\u0268\u0289'
                       + '\u1E01\u1E15\u1E17\u1E19\u1E1B\u1E1D\u1E2D\u1E2F'
                       + '\u1E4D\u1E4F\u1E51\u1E53\u1E73\u1E75\u1E77\u1E79'
                       + '\u1E7B\u1E9A\u1EA1\u1EA3\u1EA5\u1EA7\u1EA9\u1EAB'
                       + '\u1EAD\u1EAF\u1EB1\u1EB3\u1EB5\u1EB7\u1EB9\u1EBB'
                       + '\u1EBD\u1EBF\u1EC1\u1EC3\u1EC5\u1EC7\u1EC9\u1ECB'
                       + '\u1ECD\u1ECF\u1ED1\u1ED3\u1ED5\u1ED7\u1ED9\u1EDB'
                       + '\u1EDD\u1EDF\u1EE1\u1EE3\u1EE5\u1EE7\u1EE9\u1EEB'
                       + '\u1EED\u1EEF\u1EF1\uFF41\uFF4F\uFF55';

        /*
         *   A few upper-case vowels in unicode don't have lower-case
         *   mappings - consider them separately. 
         */
        local vowelsUpperOnly = '\u0130\u019f';

        /* 
         *   the various accented forms of the letter 'y' - these are all
         *   lower-case versions; the upper-case versions all map to these 
         */
        local ys = 'y\u00FD\u00FF\u0177\u01B4\u1E8F\u1E99\u1EF3'
                   + '\u1EF5\u1EF7\u1EF9\u24B4\uFF59';

        /* if there's an explicit indefinite article, use it */
        if (articleIndef != nil)
        {
            "<<articleIndef>> <<sDescStr>>";
            return;
        }

        /* check for the plural */        
        if (isPlural)
        {
            /* it's a plural - always use 'some' as the article */
            "some <<sDescStr>>";
        }
        else
        {
            local firstChar;
            local firstCharLower;

            /* if it's empty, just display "a" */
            if (sDescStr == '')
            {
                "a";
                return;
            }

            /* get the first character of the name */
            firstChar = sDescStr.substr(1, 1);
            firstCharLower = firstChar.toLower();

            /* 
             *   look for it in the lower-case and upper-case-only vowel
             *   lists 
             */
            if (vowels.find(firstCharLower) != nil
                || vowelsUpperOnly.find(firstChar) != nil)
            {
                /* it starts with a vowel */
                "an <<sDescStr>>";
            }
            else if (ys.find(firstCharLower) != nil)
            {
                local secondChar;

                /* get the second character, if there is one */
                secondChar = sDescStr.substr(2, 1);

                /* 
                 *   It starts with 'y' - if the second letter is a
                 *   consonant, assume the 'y' is a vowel sound, hence we
                 *   should use 'an'; otherwise assume the 'y' is a
                 *   dipthong 'ei' sound, which means we should use 'a'.
                 *   If there's no second character at all, or the second
                 *   character isn't alphabetic, use 'a' - "a Y" or "a
                 *   Y-connector".  
                 */
                if (secondChar == ''
                    || rexMatch('<alpha>', secondChar) == nil
                    || vowels.find(secondChar.toLower()) != nil
                    || vowelsUpperOnly.find(secondChar) != nil)
                {
                    /* 
                     *   it's just one character, or the second character
                     *   is non-alphabetic, or the second character is a
                     *   vowel - in any of these cases, use 'a' 
                     */
                    "a <<sDescStr>>";
                }
                else
                {
                    /* the second character is a consonant - use 'an' */
                    "an <<sDescStr>>";
                }
            }
            else
            {
                /* it starts with a consonant */
                "a <<sDescStr>>";
            }
        }
    }

    /*
     *   Display a default plural description.  If the short description
     *   name ends in anything other than 'y', we'll add an 's'; otherwise
     *   we'll replace the 'y' with 'ies'.  Some objects will have to
     *   override this to deal with irregular plurals ('child' ->
     *   'children' and the like).  
     */
    pluralDesc
    {
        local len;
        local lastChar;

        /* if there's no short description, display nothing */
        len = sDescStr.length();
        if (len == 0)
            return;

        /* if it's only one letter long, add an apostrophe-s */
        if (len == 1)
        {
            "<<sDescStr>>'s";
            return;
        }

        /* get the last character of the name */
        lastChar = sDescStr.substr(len, 1);

        /*
         *   if the last letter is a capital letter, it must be an
         *   abbreviation of some kind; add an apostrophe-s 
         */
        if (rexMatch('<upper>', lastChar) != nil)
        {
            "<<sDescStr>>'s";
            return;
        }

        /* if it ends with 'y', the plural is made with '-ies' */
        if (lastChar == 'y')
        {
            "<<sDescStr.substr(1, len - 1)>>ies";
            return;
        }

        /* for anything else, just add 's' */
        "<<sDescStr>>s";
    }

    /*
     *   Show this item as part of a list.  'options' is a combination of
     *   LIST_xxx flags indicating the type of listing.  'senseInfo' is a
     *   SenseInfoListEntry object giving the sense information applying
     *   to this object for the purposes of the listing.  
     */
    showListItem(options, pov, senseInfo)
    {
        /* use the appropriate display function, based on the options */
        if ((options & LIST_GROUP) != 0)
            groupListDesc(options, pov, senseInfo);
        else
            listDesc(options, pov, senseInfo);
    }

    /*
     *   Show this item as part of a list, grouped with a count of
     *   list-equivalent items.  
     */
    showListItemCounted(equivCount, options, pov, senseInfo)
    {
        /* use the appropriate display function, based on the options */
        if ((options & LIST_GROUP) != 0)
            groupCountListDesc(equivCount, options, pov, senseInfo);
        else
            countListDesc(equivCount, options, pov, senseInfo);
    }

    /*
     *   Single-item listing description.  This is used to display the
     *   item when it appears as a single (non-grouped) item in a list.
     *   By default, we just show the indefinite article description.  
     */
    listDesc(options, pov, senseInfo)
    {
        withVisualSenseInfo(pov, senseInfo, &aDesc);
    }

    /*
     *   Single-item counted listing description.  This is used to display
     *   an item with a count of equivalent items ("four gold coins").
     */
    countListDesc(equivCount, options, pov, senseInfo)
    {
        withVisualSenseInfo(pov, senseInfo, &countDesc, equivCount);
    }

    /*
     *   Group listing description.  This is used to list an item as part
     *   of its listing group.  By default, we just show the indefinite
     *   article description.  
     */
    groupListDesc(options, pov, senseInfo)
    {
        withVisualSenseInfo(pov, senseInfo, &aDesc);
    }

    /*
     *   Group counted description.  This is used to display the item in a
     *   list when it's grouped and a number of equivalent items appear in
     *   the group.
     *   
     *   For example: in "$1.53 in coins (six quarters and three
     *   pennies)", this routine is used to display the "six quarters"
     *   part.  
     */
    groupCountListDesc(equivCount, options, pov, senseInfo)
    {
        countListDesc(equivCount, options, pov, senseInfo);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Library Messages 
 */
libMessages: object
    /* generic short description of a dark room */
    roomDarkSDesc = "In the dark"

    /* generic long description of a dark room */
    roomDarkLDesc = "It's pitch black. "

    /* generic long description of a Thing */
    thingLDesc(obj) { "You see nothing unusual about <<obj.itDescObj>>. "; }

    /* 
     *   the object used to construct the sentence desribing the listing
     *   of a room's contents 
     */
    roomLister = libRoomLister

    /* 
     *   the object used to construct the sentence describing a dark
     *   room's contents 
     */
    darkRoomLister = libDarkRoomLister

    /* the object used to list the inventory for the player charater */
    playerInventoryLister = stdPlayerInventoryLister

    /* the object used to list items being worn by the player character */
    playerWearingLister = stdPlayerWearingLister

    /* the list separator character in the middle of a list */
    listSepMiddle = ", "

    /* the list separator character for a two-element list */
    listSepTwo = " and "

    /* the list separator for the end of a list of at least three elements */
    listSepEnd = ", and "

    /* 
     *   the list separator for the middle of a long list (a list with
     *   embedded lists not otherwise set off, such as by parentheses) 
     */
    longListSepMiddle = "; "

    /* the list separator for a two-element list of sublists */
    longListSepTwo = ", and "

    /* the list separator for the end of a long list */
    longListSepEnd = "; and "

    /* the status message for a LightSource that's currently turned on */
    providingLightDesc = " (providing light)"

    /* the status message for a Wearable that's currently being worn */
    wornDesc = " (being worn)"

    /* 
     *   the status message for an object that's both providing light and
     *   being worn 
     */
    providingLightWornDesc = " (providing light and being worn)"

    /*
     *   Score ranking list.  Games will almost always want to override
     *   this with a customized list.  For each entry in the list, the
     *   first element is the minimum score for the rank, and the second
     *   is a string describing the rank.  The ranks should be given in
     *   ascending order, since we simply search the list for the first
     *   item whose minimum score is greater than our score, and use the
     *   preceding item.
     *   
     *   If this is set to nil, which it is by default, we'll simply skip
     *   score ranks entirely.  
     */
    scoreRankTable = nil

    /* show the full message for a given score rank string */
    showScoreRankMessage(msg)
    {
        "This makes you <<msg>>. ";
    }

    /* show the basic score message */
    showScoreMessage(points, maxPoints, turns)
    {
        "Your score is <<points>> of a possible <<maxPoints>>,
        in <<libGlobal.totalTurns>> moves. ";
    }

    /* 
     *   show the list prefix for the full score listing; this is shown on
     *   a line by itself before the list of full score items, shown
     *   indented and one item per line 
     */
    showFullScorePrefix = "You scored:"

    /*
     *   show the item prefix, with the number of points, for a full score
     *   item - immediately after this is displayed, we'll display the
     *   description message for the achievement 
     */
    fullScoreItemPoints(points)
    {
        "\t<<points>> point<<points == 1 ? '' : 's'>> for ";
    }

    /* show the response to an empty command line */
    emptyCommandResponse() { "I beg your pardon?"; }
;

/*
 *   Plain lister - this lister doesn't show anything for an empty list,
 *   and doesn't show a list suffix or prefix. 
 */
plainLister: ShowListInterface
    /* show the prefix/suffix in wide mode */
    showListPrefixWide(itemCount, pov, parent) { }
    showListSuffixWide(itemCount, pov, parent) { }

    /* show the tall prefix */
    showListPrefixTall(itemCount, pov, parent) { }

    /* show an empty list */
    showListEmpty(pov, parent) { }
;

/*
 *   The "room lister" object - this is the object that we use by default
 *   with showList() to construct the listing of the portable items in a
 *   room when displaying the room's description.
 */
libRoomLister: ShowListInterface
    /* show the prefix/suffix in wide mode */
    showListPrefixWide(itemCount, pov, parent) { "You see "; }
    showListSuffixWide(itemCount, pov, parent) { " here. "; }

    /* show the tall prefix */
    showListPrefixTall(itemCount, pov, parent) { "You see:"; }

    /* show an empty list */
    showListEmpty(pov, parent) { }
;

/*
 *   The room lister for dark rooms 
 */
libDarkRoomLister: ShowListInterface
    showListPrefixWide(itemCount, pov, parent)
        { "In the darkness you can see "; }
    showListSuffixWide(itemCount, pov, parent) { ". "; }

    showListPrefixTall(itemCount, pov, parent)
        { "In the darkness you see:"; }
    
    showListEmpty(pov, parent) { }
;

/*
 *   Base class for inventory listers 
 */
class InventoryLister: ShowListInterface
;

/*
 *   standard inventory lister for the player character 
 */
stdPlayerInventoryLister: InventoryLister
    showListPrefixWide(itemCount, pov, parent) { "You are carrying "; }
    showListSuffixWide(itemCount, pov, parent) { ". "; }

    showListPrefixTall(itemCount, pov, parent) { "You are carrying:"; }

    showListEmpty(pov, parent) { "You are empty-handed."; }
;

/*
 *   Base class for non-player character listers
 */
class NonPlayerInventoryLister: InventoryLister
    showListPrefixWide(itemCount, pov, parent)
        { "\^<<parent.nameIsDesc>> carrying "; }
    showListSuffixWide(itemCount, parent) { ". "; }

    showListPrefixTall(itemCount, pov, parent)
        { "\^<<parent.nameIsDesc>> carrying:"; }

    showListEmpty(pov, parent)
        { "\^<<parent.nameIsDesc>> empty-handed. "; }
;

/*
 *   Base class for inventory lister for items being worn 
 */
class WearingLister: ShowListInterface
;

/*
 *   standard inventory lister for items being worn by the player
 *   character 
 */
stdPlayerWearingLister: WearingLister
    showListPrefixWide(itemCount, pov, parent) { "You are wearing "; }
    showListSuffixWide(itemCount, pov, parent) { ". "; }

    showListPrefixTall(itemCount, pov, parent) { "You are wearing:"; }

    showListEmpty(pov, parent) { }
;

/*
 *   Base class for non-player character inventory listers for items being
 *   worn by NPC's 
 */
class NonPlayerWearingLister: WearingLister
    showListPrefixWide(itemCount, pov, parent)
    {
        "\^<<parent.nameIsDesc>> wearing ";
    }
    showListSuffixWide(itemCount, pov, parent) { ". "; }

    showListPrefixTall(itemCount, pov, parent)
    {
        "\^<<parent.nameIsDesc>> wearing:";
    }
    
    showListEmpty(pov, parent) { }
;

/*
 *   Base class for contents listers 
 */
class ContentsLister: ShowListInterface
    showListEmpty(pov, parent) { }
    showListSuffixWide(itemCount, pov, parent) { ". "; }
;

/*
 *   Contents lister for things
 */
thingContentsLister: ContentsLister
    showListPrefixWide(itemCount, pov, parent)
    {
        "\^<<parent.theDesc>> contain<<parent.isPlural ? '' : 's'>> ";
    }
    showListPrefixTall(itemCount, pov, parent)
    {
        "\^<<parent.theDesc>> contain<<parent.isPlural ? '' : 's'>>:";
    }
;

/*
 *   Contents lister for a surface 
 */
surfaceContentsLister: ContentsLister
    showListPrefixWide(itemCount, pov, parent)
    {
        "On <<parent.theDesc>> you see ";
    }
    showListPrefixTall(itemCount, pov, parent)
    {
        "On <<parent.theDesc>>:";
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Spell out an integer number.
 *   
 *   This entire function is language-specific.  We have not attempted to
 *   break down the numeric conversion into smaller language-specific
 *   primitives, because we assume that the spelled-out form of numbers
 *   varies considerably from language to language and that it would be
 *   difficult to cover the full range of possibilities without simply
 *   making the entire function language-specific.
 *   
 *   Note that some of the SPELLINT_xxx flags might not be meaningful in
 *   all languages, because most of the flags are by their very nature
 *   associated with language-specific idioms.  Translations are free to
 *   ignore flags that indicate variations with no local equivalent.  
 */
spellIntExt(val, flags)
{
    local str;
    local trailingSpace;
    local needAnd;
    local powers = [1000000000, ' billion ',
                    1000000,    ' million ',
                    1000,       ' thousand ',
                    100,        ' hundred '];

    /* start with an empty string */
    str = '';
    trailingSpace = nil;
    needAnd = nil;

    /* if it's zero, it's a special case */
    if (val == 0)
        return 'zero';

    /* 
     *   if the number is negative, note it in the string, and use the
     *   absolute value 
     */
    if (val < 0)
    {
        str = 'minus ';
        val = -val;
    }

    /* do each named power of ten */
    for (local i = 1 ; val >= 100 && i <= powers.length() ; i += 2)
    {
        /* 
         *   if we're in teen-hundreds mode, do the teen-hundreds - this only
         *   works for values from 1,100 to 9,999, since a number like 12,000
         *   doesn't work this way - 'one hundred twenty hundred' is no good 
         */
        if ((flags & SPELLINT_TEEN_HUNDREDS) != 0
            && val >= 1100 && val < 10000)
        {
            /* if desired, add a comma if there was a prior power group */
            if (needAnd && (flags & SPELLINT_COMMAS) != 0)
                str = str.substr(1, str.length() - 1) + ', ';
            
            /* spell it out as a number of hundreds */
            str += spellIntExt(val / 100, flags) + ' hundred ';

            /* take off the hundreds */
            val %= 100;

            /* note the trailing space */
            trailingSpace = true;

            /* we have something to put an 'and' after, if desired */
            needAnd = true;

            /* 
             *   whatever's left is below 100 now, so there's no need to
             *   keep scanning the big powers of ten
             */
            break;
        }

        /* if we have something in this power range, apply it */
        if (val >= powers[i])
        {
            /* if desired, add a comma if there was a prior power group */
            if (needAnd && (flags & SPELLINT_COMMAS) != 0)
                str = str.substr(1, str.length() - 1) + ', ';

            /* add the number of multiples of this power and the power name */
            str += spellIntExt(val / powers[i], flags) + powers[i+1];

            /* take it out of the remaining value */
            val %= powers[i];

            /* 
             *   note that we have a trailing space in the string (all of
             *   the power-of-ten names have a trailing space, to make it
             *   easy to tack on the remainder of the value) 
             */
            trailingSpace = true;

            /* we have something to put an 'and' after, if one is desired */
            needAnd = true;
        }
    }

    /* 
     *   if we have anything left, and we have written something so far,
     *   and the caller wanted an 'and' before the tens part, add the
     *   'and' 
     */
    if ((flags & SPELLINT_AND_TENS) != 0
        && needAnd
        && val != 0)
    {
        /* add the 'and' */
        str += 'and ';
        trailingSpace = true;
    }

    /* do the tens */
    if (val >= 20)
    {
        /* anything above the teens is nice and regular */
        str += ['twenty', 'thirty', 'forty', 'fifty', 'sixty',
                'seventy', 'eighty', 'ninety'][val/10 - 1];
        val %= 10;

        /* if it's non-zero, we'll add the units, so add a hyphen */
        if (val != 0)
            str += '-';

        /* we no longer have a trailing space in the string */
        trailingSpace = nil;
    }
    else if (val >= 10)
    {
        /* we have a teen */
        str += ['ten', 'eleven', 'twelve', 'thirteen', 'fourteen',
                'fifteen', 'sixteen', 'seventeen', 'eighteen',
                'nineteen'][val - 9];

        /* we've finished with the number */
        val = 0;

        /* there's no trailing space */
        trailingSpace = nil;
    }

    /* if we have a units value, add it */
    if (val != 0)
    {
        /* add the units name */
        str += ['one', 'two', 'three', 'four', 'five',
                'six', 'seven', 'eight', 'nine'][val];

        /* we have no trailing space now */
        trailingSpace = nil;
    }

    /* if there's a trailing space, remove it */
    if (trailingSpace)
        str = str.substr(1, str.length() - 1);

    /* return the string */
    return str;
}

