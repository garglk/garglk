/* Copyright (c) 1988-2000 by Michael J. Roberts.  All Rights Reserved. */
/*
   adv.t  - standard adventure definitions for TADS games
   Version 2.5.17
   
   This file is part of TADS:  The Text Adventure Development System.
   Please see the file LICENSE.TAD (which should be part of the TADS
   distribution) for information on copying and using TADS.

   You may modify and use this file in any way you want, provided that
   if you redistribute modified copies of this file in source form, the
   copies must include the original copyright notice (including this
   paragraph), and must be clearly marked as modified from the original
   version.

   This file defines the basic classes and functions used by most TADS
   adventure games.  It is generally #include'd at the start of each game
   source file.
*/

/* parse adv.t using normal TADS operators */
#pragma C-


/* ------------------------------------------------------------------------ */
/*
 *   Constants for parser status codes.  These codes are passed to
 *   parseError and parseErrorParam to indicate which message these
 *   routines should display.  In addition, these codes are returned by
 *   parserResolveObjects.
 */

#define PRS_SUCCESS           0                                  /* success */

#define PRSERR_BAD_PUNCT      1       /* I don't understand the punctuation */
#define PRSERR_UNK_WORD       2                    /* I don't know the word */
#define PRSERR_TOO_MANY_OBJS  3      /* The word refers to too many objects */
#define PRSERR_ALL_OF         4    /* You left something out after "all of" */
#define PRSERR_BOTH_OF        5   /* You left something out after "both of" */
#define PRSERR_OF_NOUN        6               /* Expected a noun after "of" */
#define PRSERR_ART_NOUN       7    /* An article must be followed by a noun */
#define PRSERR_DONT_SEE_ANY   9                  /* I don't see any %s here */
#define PRSERR_TOO_MANY_OBJS2 10   /* Referring to too many objects with %s */
#define PRSERR_TOO_MANY_OBJS3 11           /* Referring to too many objects */
#define PRSERR_ONE_ACTOR      12        /* You can only speak to one person */
#define PRSERR_DONT_KNOW_REF  13     /* Don't know what you're referring to */
#define PRSERR_DONT_KNOW_REF2 14     /* Don't know what you're referring to */
#define PRSERR_DONT_SEE_REF   15      /* Don't see what you're referring to */
#define PRSERR_DONT_SEE_ANY2  16               /* You don't see any %s here */
#define PRSERR_NO_MULTI_IO    25     /* can't use multiple indirect objects */
#define PRSERR_NO_AGAIN       26            /* There's no command to repeat */
#define PRSERR_BAD_AGAIN      27           /* You can't repeat that command */
#define PRSERR_NO_MULTI       28   /* can't use multiple objects w/this cmd */
#define PRSERR_ANY_OF         29   /* You left something out after "any of" */
#define PRSERR_ONLY_SEE       30                  /* I only see %d of those */
#define PRSERR_CANT_TALK      31                  /* You can't talk to that */
#define PRSERR_INT_PPC_INV    32      /* internal: invalid preparseCmd list */
#define PRSERR_INT_PPC_LONG   33  /* internal: preparseCmd command too long */
#define PRSERR_INT_PPC_LOOP   34              /* internal: preparseCmd loop */
#define PRSERR_DONT_SEE_ANY_MORE 38     /* You don't see that here any more */
#define PRSERR_DONT_SEE_THAT  39                 /* You don't see that here */

#define PRSERR_NO_NEW_NUM     40        /* can't create new numbered object */
#define PRSERR_BAD_DIS_STAT   41        /* invalid status from disambigXobj */
#define PRSERR_EMPTY_DISAMBIG 42        /* empty response to disambig query */
#define PRSERR_DISAMBIG_RETRY 43   /* retry object disambig response as cmd */
#define PRSERR_AMBIGUOUS      44                 /* objects still ambiguous */
#define PRSERR_OOPS_UPDATE    45             /* command corrected by "oops" */

#define PRSERR_TRY_AGAIN      100                  /* Let's try it again... */
#define PRSERR_WHICH_PFX      101                /* Which %s do you mean... */
#define PRSERR_WHICH_COMMA    102                                /* (comma) */
#define PRSERR_WHICH_OR       103                               /* ...or... */
#define PRSERR_WHICH_QUESTION 104                        /* (question mark) */

#define PRSERR_DONTKNOW_PFX   110                 /* I don't know how to... */
#define PRSERR_DONTKNOW_SPC   111                                /* (space) */
#define PRSERR_DONTKNOW_ANY   112                            /* ...anything */
#define PRSERR_DONTKNOW_TO    113                               /* ...to... */
#define PRSERR_DONTKNOW_SPC2  114                                /* (space) */
#define PRSERR_DONTKNOW_END   115                               /* (period) */

#define PRSERR_MULTI          120     /* (colon) for multiple-object prefix */

#define PRSERR_ASSUME_OPEN    130     /* (open paren) for defaulted objects */
#define PRSERR_ASSUME_CLOSE   131                          /* (close paren) */
#define PRSERR_ASSUME_SPC     132                                /* (space) */

#define PRSERR_WHAT_PFX       140                 /* What do you want to... */
#define PRSERR_WHAT_IT        141                               /* ...it... */
#define PRSERR_WHAT_TO        142                               /* ...to... */
#define PRSERR_WHAT_END       143                        /* (question mark) */
#define PRSERR_WHAT_THEM      144                             /* ...them... */
#define PRSERR_WHAT_HIM       145                              /* ...him... */
#define PRSERR_WHAT_HER       146                              /* ...her... */
#define PRSERR_WHAT_THEM2     147                             /* ...them... */
#define PRSERR_WHAT_PFX2      148                    /* What do you want... */
#define PRSERR_WHAT_TOSPC     149                               /* ...to... */

#define PRSERR_MORE_SPECIFIC  160        /* you'll have to be more specific */

#define PRSERR_NOREACH_MULTI  200     /* (colon) for prefixing unreachables */


/* ------------------------------------------------------------------------ */
/* 
 *   constants for the event codes returned by the inputevent() intrinsic
 *   function 
 */
#define INPUT_EVENT_KEY        1
#define INPUT_EVENT_TIMEOUT    2
#define INPUT_EVENT_HREF       3
#define INPUT_EVENT_NOTIMEOUT  4
#define INPUT_EVENT_EOF        5

/*
 *   constants for inputdialog() 
 */
#define INDLG_OK               1
#define INDLG_OKCANCEL         2
#define INDLG_YESNO            3
#define INDLG_YESNOCANCEL      4

#define INDLG_ICON_NONE        0
#define INDLG_ICON_WARNING     1
#define INDLG_ICON_INFO        2
#define INDLG_ICON_QUESTION    3
#define INDLG_ICON_ERROR       4

#define INDLG_LBL_OK           1
#define INDLG_LBL_CANCEL       2
#define INDLG_LBL_YES          3
#define INDLG_LBL_NO           4



/*
 *   constants for gettime() type codes 
 */
#define GETTIME_DATE_AND_TIME  1
#define GETTIME_TICKS          2

/*
 *   constants for askfile() prompt type codes 
 */
#define ASKFILE_PROMPT_OPEN    1       /* open an existing file for reading */
#define ASKFILE_PROMPT_SAVE    2                        /* save to the file */

/*
 *   constants for askfile() flags 
 */
#define ASKFILE_EXT_RESULT     1                   /* extended result codes */

/*
 *   askfile() return codes - these are returned in the first element of
 *   the list returned from askfile() when ASKFILE_EXT_RESULT is used in
 *   the 'flags' parameter 
 */
#define ASKFILE_SUCCESS        0  /* success - 2nd list element is filename */
#define ASKFILE_FAILURE        1     /* an error occurred asking for a file */
#define ASKFILE_CANCEL         2       /* player canceled the file selector */


/*
 *   constants for askfile() file type codes 
 */
#define FILE_TYPE_GAME    0                      /* a game data file (.gam) */
#define FILE_TYPE_SAVE    1                          /* a saved game (.sav) */
#define FILE_TYPE_LOG     2                      /* a transcript (log) file */
#define FILE_TYPE_DATA    4         /* general data file (used for fopen()) */
#define FILE_TYPE_CMD     5                           /* command input file */
#define FILE_TYPE_TEXT    7                                    /* text file */
#define FILE_TYPE_BIN     8                             /* binary data file */
#define FILE_TYPE_UNKNOWN 11                           /* unknown file type */

/*
 *   constants for execCommand() flags 
 */
#define EC_HIDE_SUCCESS   1                        /* hide success messages */
#define EC_HIDE_ERROR     2                          /* hide error messages */
#define EC_SKIP_VALIDDO   4                /* skip direct object validation */
#define EC_SKIP_VALIDIO   8              /* skip indirect object validation */

/* 
 *   constants for execCommand() return codes
 */
#define EC_SUCCESS        0                        /* successful completion */
#define EC_EXIT           1013                           /* "exit" executed */
#define EC_ABORT          1014                          /* "abort" executed */
#define EC_ASKDO          1015                          /* "askdo" executed */
#define EC_ASKIO          1016                          /* "askio" executed */
#define EC_EXITOBJ        1019                        /* "exitobj" executed */
#define EC_INVAL_SYNTAX   1200                /* invalid sentence structure */
#define EC_VERDO_FAILED   1201                          /* verDoVerb failed */
#define EC_VERIO_FAILED   1202                          /* verIoVerb failed */
#define EC_NO_VERDO       1203               /* no verDoVerb method defined */
#define EC_NO_VERIO       1204               /* no verIoVerb method defined */
#define EC_INVAL_DOBJ     1205           /* direct object validation failed */
#define EC_INVAL_IOBJ     1206         /* indirect object validation failed */

/*
 *   constants for parserGetObj object codes 
 */
#define PO_ACTOR    1                                              /* actor */
#define PO_VERB     2                                    /* deepverb object */
#define PO_DOBJ     3                                      /* direct object */
#define PO_PREP     4   /* preposition object (introducing indirect object) */
#define PO_IOBJ     5                                    /* indirect object */
#define PO_IT       6                                /* get the "it" object */
#define PO_HIM      7                               /* get the "him" object */
#define PO_HER      8                               /* get the "her" object */
#define PO_THEM     9                              /* get the "them" object */

/*
 *   constants for parseNounPhrase return codes 
 */
#define PNP_ERROR        1                      /* noun phrase syntax error */
#define PNP_USE_DEFAULT  2                        /* use default processing */

/*
 *   constants for disambigDobj and disambigIobj status codes 
 */
#define DISAMBIG_CONTINUE    1     /* continue disambiguation for this list */
#define DISAMBIG_DONE        2                    /* list is fully resolved */
#define DISAMBIG_ERROR       3      /* disambiguation failed; abort command */
#define DISAMBIG_PARSE_RESP  4         /* parse interactive response string */
#define DISAMBIG_PROMPTED    5 /* prompted for response, but didn't read it */

/*
 *   Parser word types.  These values may be combined, since a given word
 *   may appear in the dictionary with multiple types.  To test for a
 *   particular type, use the bitwise AND operator:
 *   
 *   ((typ & PRSTYP_NOUN) != 0).  
 */
#define PRSTYP_ARTICLE  1                                     /* the, a, an */
#define PRSTYP_ADJ      2                                      /* adjective */
#define PRSTYP_NOUN     4                                           /* noun */
#define PRSTYP_PREP     8                                    /* preposition */
#define PRSTYP_VERB     16                                          /* verb */
#define PRSTYP_SPEC     32          /* special words - "of", ",", ".", etc. */
#define PRSTYP_PLURAL   64                                        /* plural */
#define PRSTYP_UNKNOWN  128                    /* word is not in dictionary */

/*
 *   Parser noun-phrase flags.  These values may be combined; to test for
 *   a single flag, use the bitwise AND operator:
 *   
 *   ((flag & PRSFLG_UNKNOWN) != 0) 
 */
#define PRSFLG_ALL      1                                          /* "all" */
#define PRSFLG_EXCEPT   2   /* "except" or "but" (used in "all except ...") */
#define PRSFLG_IT       4                                           /* "it" */
#define PRSFLG_THEM     8                                         /* "them" */
#define PRSFLG_NUM      16                                      /* a number */
#define PRSFLG_COUNT    32          /* noun phrase uses a count - "3 coins" */
#define PRSFLG_PLURAL   64   /* noun phrase is a plural usage - "the coins" */
#define PRSFLG_ANY      128          /* noun phrase uses "any" - "any coin" */
#define PRSFLG_HIM      256                                        /* "him" */
#define PRSFLG_HER      512                                        /* "her" */
#define PRSFLG_STR      1024                             /* a quoted string */
#define PRSFLG_UNKNOWN  2048        /* noun phrase contains an unknown word */
#define PRSFLG_ENDADJ   4096            /* noun phrase ends in an adjective */
#define PRSFLG_TRUNC    8192           /* noun phrase uses a truncated word */


/*
 *   Constants for parserResolveObjects usage codes 
 */
#define PRO_RESOLVE_DOBJ  1                                /* direct object */
#define PRO_RESOLVE_IOBJ  2                              /* indirect object */
#define PRO_RESOLVE_ACTOR 3                                        /* actor */


/*
 *   Constants for result codes from restore() 
 */
#define RESTORE_SUCCESS          0                               /* success */
#define RESTORE_FILE_NOT_FOUND   1                        /* file not found */
#define RESTORE_NOT_SAVE_FILE    2                 /* not a saved game file */
#define RESTORE_BAD_FMT_VSN      3      /* incompatible file format version */
#define RESTORE_BAD_GAME_VSN     4 /* file saved by another game or version */
#define RESTORE_READ_ERROR       5           /* error reading from the file */
#define RESTORE_NO_PARAM_FILE    6    /* no parameter file for restore(nil) */

/*
 *   Constants for defined() function.  When passed as the optional third
 *   argument to defined(), these flags specify what type of information
 *   is returned from defined().
 *   
 *   By default, defined(obj, &prop) returns true if the property is
 *   defined or inherited by the object, nil if not.  The DEFINED_ANY flag
 *   has the same effect.
 *   
 *   When DEFINED_DIRECTLY is used, the function returns true only if the
 *   property is directly defined by the object, not merely inherited from
 *   a superclass.
 *   
 *   When DEFINED_INHERITS is used, the function returns true only if the
 *   property is inherited from a superclass, and returns nil if the
 *   property isn't defined at all or is defined directly by the object.
 *   
 *   When DEFINED_GET_CLASS is used, the function returns the object that
 *   actually defines the property; this will be the object itself if the
 *   object overrides the property, otherwise it will be the superclass
 *   from which the object inherits the property.  The function returns
 *   nil if the property isn't defined or inherited at all for the object.
 */
#define DEFINED_ANY         1  /* default: property is defined or inherited */
#define DEFINED_DIRECTLY    2             /* defined directly by the object */
#define DEFINED_INHERITS    3            /* inherited, not defined directly */
#define DEFINED_GET_CLASS   4                     /* get the defining class */

/*
 *   Constants for datatype() and proptype() 
 */
#define DTY_NUMBER  1                                             /* number */
#define DTY_OBJECT  2                                             /* object */
#define DTY_SSTRING 3                         /* single-quoted string value */
#define DTY_NIL     5                                                /* nil */
#define DTY_CODE    6                                        /* method code */
#define DTY_LIST    7                                               /* list */
#define DTY_TRUE    8                                               /* true */
#define DTY_DSTRING 9                               /* double-quoted string */
#define DTY_FUNCPTR 10                                  /* function pointer */
#define DTY_PROP    13                                  /* property pointer */


/* ------------------------------------------------------------------------ */
/*
 *   Define compound prepositions.  Since prepositions that appear in
 *   parsed sentences must be single words, we must define any logical
 *   prepositions that consist of two or more words here.  Note that
 *   only two words can be pasted together at once; to paste more, use
 *   a second step.  For example,  'out from under' must be defined in
 *   two steps:
 *
 *     compoundWord 'out' 'from' 'outfrom';
 *     compoundWord 'outfrom' 'under' 'outfromunder';
 *
 *   Listed below are the compound prepositions that were built in to
 *   version 1.0 of the TADS run-time.
 */
compoundWord 'on' 'to' 'onto';           /* on to --> onto */
compoundWord 'in' 'to' 'into';           /* in to --> into */
compoundWord 'in' 'between' 'inbetween'; /* and so forth */
compoundWord 'down' 'in' 'downin';
compoundWord 'down' 'on' 'downon';
compoundWord 'up' 'on' 'upon';
compoundWord 'out' 'of' 'outof';
compoundWord 'off' 'of' 'offof';
compoundWord 'i' 'wide' 'i_wide';
compoundWord 'invent' 'wide' 'i_wide';
compoundWord 'inventory' 'wide' 'i_wide';
compoundWord 'i' 'tall' 'i_tall';
compoundWord 'invent' 'tall' 'i_tall';
compoundWord 'inventory' 'tall' 'i_tall';
;

/*
 *   Format strings:  these associate keywords with properties.  When
 *   a keyword appears in output between percent signs (%), the matching
 *   property of the current command's actor is evaluated and substituted
 *   for the keyword (and the percent signs).  For example, if you have:
 *
 *      formatstring 'you' fmtYou;
 *
 *   and the command being processed is:
 *
 *      fred, pick up the paper
 *
 *   and the "fred" actor has fmtYou = "he", and this string is output:
 *
 *      "%You% can't see that here."
 *
 *   Then the actual output is:  "He can't see that here."
 *
 *   The format strings are chosen to look like normal output (minus the
 *   percent signs, of course) when the actor is the player character (Me).
 */
formatstring 'you' fmtYou;
formatstring 'your' fmtYour;
formatstring 'you\'re' fmtYoure;
formatstring 'youm' fmtYoum;
formatstring 'you\'ve' fmtYouve;
formatstring 's' fmtS;
formatstring 'es' fmtEs;
formatstring 'have' fmtHave;
formatstring 'do' fmtDo;
formatstring 'are' fmtAre;
formatstring 'me' fmtMe;
;

/*
 *   Special Word List: This list defines the special words that the
 *   parser needs for input commands.  If the list is not provided, the
 *   parser uses the old defaults.  The list below is the same as the old
 *   defaults.  Note - the words in this list must appear in the order
 *   shown below.
 */
specialWords
    'of',                        /* used in phrases such as "piece of paper" */
    'and',             /* conjunction for noun lists or to separate commands */
    'then',                              /* conjunction to separate commands */
    'all' = 'everything',               /* refers to every accessible object */
    'both',      /* used with plurals, or to answer disambiguation questions */
    'but' = 'except',                      /* used to exclude items from ALL */
    'one',                       /* used to answer questions:  "the red one" */
    'ones',                        /* likewise for plurals:  "the blue ones" */
    'it' = 'there',              /* refers to last single direct object used */
    'them',                             /* refers to last direct object list */
    'him',                       /* refers to last masculine actor mentioned */
    'her',                        /* refers to last feminine actor mentioned */
    'any' = 'either'         /* pick object arbitrarily from ambiguous list */
;

/*
 *   Forward-declare functions.  This is not required in most cases,
 *   but it doesn't hurt.  Providing these forward declarations ensures
 *   that the compiler knows that we want these symbols to refer to
 *   functions rather than objects.
 */
checkDoor: function;
checkReach: function;
itemcnt: function;
isIndistinguishable: function;
sayPrefixCount: function;
listcont: function;
nestlistcont: function;
listcontcont: function;
turncount: function;
addweight: function;
addbulk: function;
incscore: function;
darkTravel: function;
scoreRank: function;
terminate: function;
goToSleep: function;
initSearch: function;
reachableList: function;
initRestart: function;
contentsListable: function;
;

/*
 *  inputline: function
 *
 *  This is a simple cover function for the built-in function input().
 *  This cover function switches to the 'TADS-Input' font when running in
 *  HTML mode, so that input explicitly read through this function appears
 *  in the same input font that is used for normal command-line input.
 */
inputline: function
{
    local ret;
    local html_mode;

    /* note whether we're in HTML mode */
    html_mode := (systemInfo(__SYSINFO_SYSINFO) = true
                  && systemInfo(__SYSINFO_HTML_MODE));

    /* if in HTML mode, switch to TADS-Input font */
    if (html_mode)
        "<font face='TADS-Input'>";

    /* read the input */
    ret := input();

    /* exit TADS-Input font mode if appropriate */
    if (html_mode)
        "</font>";

    /* return the line of text */
    return ret;
}

/*
 *  _rand: function(x)
 *
 *  This is a modified version of the built-in random number generator, which
 *  may improve upon the standard random number generator's statistical
 *  performance in many cases.  To use this replacement, simply call _rand
 *  instead of rand in your code.
 */
_rand: function(x)
{
    return (((rand(16*x)-1)/16)+1);
}

/*
 *   initRestart - flag when a restart has occurred by setting a flag
 *   in global.
 */
initRestart: function(parm)
{
    global.restarting := true;
}

/*
 *   checkDoor:  if the door d is open, this function silently returns
 *   the room r.  Otherwise, print a message ("The door is closed.") and
 *   return nil.
 */
checkDoor: function(d, r)
{
    if (d.isopen)
         return r;
    else
    {
        setit(d);
        caps(); d.thedesc; " <<d.isdesc>> closed. ";
        return nil;
    }
}

/*
 *   checkReach: determines whether the object obj can be reached by
 *   actor in location loc, using the verb v.  This routine returns true
 *   if obj is a special object (numObj or strObj), if obj is in actor's
 *   inventory or actor's location, or if it's in the 'reachable' list for
 *   loc.  
 */
checkReach: function(loc, actor, v, obj)
{
    if (obj = numObj or obj = strObj)
        return;
    if (not (actor.isCarrying(obj) or obj.isIn(actor.location)))
    {
        if (find(loc.reachable, obj) <> nil)
             return;
        "%You% can't reach "; obj.thedesc; " from "; loc.thedesc; ". ";
        exit;
    }
}

/*
 *  isIndistinguishable: function(obj1, obj2)
 *
 *  Returns true if the two objects are indistinguishable for the purposes
 *  of listing.  The two objects are equivalent if they both have the
 *  isEquivalent property set to true, they both have the same immediate
 *  superclass, and their other listing properties match (in particular,
 *  isworn and (islamp and islit) match for both objects).
 */
isIndistinguishable: function(obj1, obj2)
{
    return (firstsc(obj1) = firstsc(obj2)
            and obj1.isworn = obj2.isworn
            and ((obj1.islamp and obj1.islit)
                 = (obj2.islamp and obj2.islit)));
}


/*
 *  itemcnt: function(list)
 *
 *  Returns a count of the "listable" objects in list.  An
 *  object is listable (that is, it shows up in a room's description)
 *  if its isListed property is true.  This function is
 *  useful for determining how many objects (if any) will be listed
 *  in a room's description.  Indistinguishable items are counted as
 *  a single item (two items are indistinguishable if they both have
 *  the same immediate superclass, and their isEquivalent properties
 *  are both true.
 */
itemcnt: function(list)
{
    local cnt, tot, i, obj, j;

    tot := length(list);
    for (i := 1, cnt := 0 ; i <= tot ; ++i)
    {
        /* only consider this item if it's to be listed */
        obj := list[i];
        if (obj.isListed)
        {
            /*
             *   see if there are other equivalent items later in the
             *   list - if so, don't count it (this ensures that each such
             *   item is counted only once, since only the last such item
             *   in the list will be counted) 
             */
            if (obj.isEquivalent)
            {
                local sc;
                
                sc := firstsc(obj);
                for (j := i + 1 ; j <= tot ; ++j)
                {
                    if (isIndistinguishable(obj, list[j]))
                        goto skip_this_item;
                }
            }
            
            /* count this item */
            ++cnt;
            
        skip_this_item: ;
        }
    }
    return cnt;
}

/*
 *  sayPrefixCount: function(cnt)
 *
 *  This function displays a count (suitable for use in listcont when
 *  showing the number of equivalent items.  We display the count spelled out
 *  if it's a small number, otherwise we just display the digits of the
 *  number.
 */
sayPrefixCount: function(cnt)
{
    if (cnt <= 20)
        say(['one' 'two' 'three' 'four' 'five'
            'six' 'seven' 'eight' 'nine' 'ten'
            'eleven' 'twelve' 'thirteen' 'fourteen' 'fifteen'
            'sixteen' 'seventeen' 'eighteen' 'nineteen' 'twenty'][cnt]);
    else
        say(cnt);
}

/*
 *  listcontgen: function(obj, flags, indent)
 *
 *  This is a general-purpose object lister routine; the other object lister
 *  routines call this routine to do their work.  This function can take an
 *  object, in which case it lists the contents of the object, or it can take
 *  a list, in which case it simply lists the items in the list.  The flags
 *  parameter specifies what to do.  LCG_TALL makes the function display a "tall"
 *  listing, with one object per line; if LCG_TALL isn't specified, the function
 *  displays a "wide" listing, showing the objects as a comma-separated list.
 *  When LCG_TALL is specified, the indent parameter indicates how many
 *  tab levels to indent the current level of listing, allowing recursive calls
 *  to display the contents of listed objects.  LCG_CHECKVIS makes the function
 *  check the visibility of the top-level object before listing its contents.
 *  LCG_RECURSE indicates that we should recursively list the contents of the
 *  objects we list; we'll use the same flags on recursive calls, and use one
 *  higher indent level.  LCG_CHECKLIST specifies that we should check
 *  the listability of any recursive contents before listing them, using
 *  the standard contentsListable() test.  To specify multiple flags, combine
 *  them with the bitwise-or (|) operator.
 */
#define LCG_TALL       1
#define LCG_CHECKVIS   2
#define LCG_RECURSE    4
#define LCG_CHECKLIST  8
listcontgen: function(obj, flags, indent)
{
    local i, count, tot, list, cur, disptot, prefix_count;

    /*
     *   Get the list.  If the "obj" parameter is already a list, use it
     *   directly; otherwise, list the contents of the object. 
     */
    switch(datatype(obj))
    {
    case 2:
        /* it's an object - list its contents */
        list := obj.contents;

        /* 
         *   if the CHECKVIS flag is specified, check to make sure the
         *   contents of the object are visible; if they're not, don't
         *   list anything 
         */
        if ((flags & LCG_CHECKVIS) != 0)
        {
            local contvis;

            /* determine whether the contents are visible */
            contvis := (!isclass(obj, openable)
                        || (isclass(obj, openable) && obj.isopen)
                        || obj.contentsVisible);

            /* if they're not visible, don't list the contents */
            if (!contvis)
                return;
        }
        break;
        
    case 7:
        /* it's already a list */
        list := obj;
        break;

    default:
        /* ignore other types entirely */
        return;
    }

    /* count the items in the list */
    tot := length(list);

    /* we haven't displayed anything yet */
    count := 0;

    /* 
     *   Count how many items we're going to display -- this may be fewer
     *   than the number of items in the list, because some items might
     *   not be listed at all (isListed = nil), and we'll list each group
     *   of equivalent objects with a single display item (with a count of
     *   the number of equivalent objets) 
     */
    disptot := itemcnt(list);

    /* iterate through the list */
    for (i := 1 ; i <= tot ; ++i)
    {
        /* get the current object */
        cur := list[i];

        /* if this object is to be listed, figure out how to show it */
        if (cur.isListed)
        {
            /* presume there is only one such object (i.e., no equivalents) */
            prefix_count := 1;
            
            /*
             *   if this is one of more than one equivalent items, list it
             *   only if it's the first one, and show the number of such
             *   items along with the first one 
             */
            if (cur.isEquivalent)
            {
                local before, after;
                local j;
                local sc;

                /* get this object's superclass */
                sc := firstsc(cur);

                /* scan for other objects equivalent to this one */
                for (before := after := 0, j := 1 ; j <= tot ; ++j)
                {
                    if (isIndistinguishable(cur, list[j]))
                    {
                        if (j < i)
                        {
                            /*
                             *   note that objects precede this one, and
                             *   then look no further, since we're just
                             *   going to skip this item anyway
                             */
                            ++before;
                            break;
                        }
                        else
                            ++after;
                    }
                }
                
                /*
                 *   if there are multiple such objects, and this is the
                 *   first such object, list it with the count prefixed;
                 *   if there are multiple and this isn't the first one,
                 *   skip it; otherwise, go on as normal 
                 */
                if (before = 0)
                    prefix_count := after;
                else
                    continue;
            }

            /* display the appropriate separator before this item */
            if ((flags & LCG_TALL) != 0)
            {
                local j;
                
                /* tall listing - indent to the desired level */
                "\n";
                for (j := 1; j <= indent; ++j)
                    "\t";
            }
            else
            {
                /* 
                 *   "wide" (paragraph-style) listing - add a comma, and
                 *   possibly "and", if this isn't the first item
                 */
                if (count > 0)
                {
                    if (count+1 < disptot)
                        ", ";
                    else if (count = 1)
                        " and ";
                    else
                        ", and ";
                }
            }
            
            /* list the object, along with the number of such items */
            if (prefix_count = 1)
            {
                /* there's only one object - show the singular description */
                cur.adesc;
            }
            else
            {
                /* 
                 *   there are multiple equivalents for this object -
                 *   display the number of the items and the plural
                 *   description 
                 */
                sayPrefixCount(prefix_count); " ";
                cur.pluraldesc;
            }
            
            /* show any additional information about the item */
            if (cur.isworn)
                " (being worn)";
            if (cur.islamp and cur.islit)
                " (providing light)";

            /* increment the number of displayed items */
            ++count;

            /* 
             *   If this is a "tall" listing, and there's at least one item
             *   contained inside the current item, list the item's
             *   contents recursively, indented one level deeper.
             *   
             *   If the 'check visibility' flag is set, then only proceed
             *   with the sublisting if the contents are listable.  
             */
            if ((flags & LCG_RECURSE) != 0
                && itemcnt(cur.contents) != 0
                && ((flags & LCG_CHECKLIST) = 0 || contentsListable(cur)))
            {
                /* 
                 *   if this is a "wide" listing, show the contents in
                 *   parentheses 
                 */
                if ((flags & LCG_TALL) = 0)
                {
                    if (cur.issurface)
                        " (upon which %you% see%s% ";
                    else
                        " (which contains ";
                }
                
                /* show the recursive listing, indented one level deeper */
                listcontgen(cur, flags, indent + 1);

                /* close the parenthetical, if we opened one */
                if ((flags & LCG_TALL) = 0)
                    ")";
            }
        }
    }
}

/*
 *  listcont: function(obj)
 *
 *  This function displays the contents of an object, separated by
 *  commas.  The thedesc properties of the contents are used.
 *  It is up to the caller to provide the introduction to the list
 *  (usually something to the effect of "The box contains" is
 *  displayed before calling listcont) and finishing the
 *  sentence (usually by displaying a period).  An object is listed
 *  only if its isListed property is true.  If there are
 *  multiple indistinguishable items in the list, the items are
 *  listed only once (with the number of the items).
 */
listcont: function(obj)
{
    /* use the general-purpose contents lister to show a "wide" listing */
    listcontgen(obj, 0, 0);
}

/*
 *  nestlistcont: function(obj, depth)
 *
 *  This function will produce a nested listing of the contents of obj.
 *  It will recurse down into the objects contents until the list is
 *  exhausted. An object's contents are listed only if they are
 *  considered visible by the normal visibility rules.  The depth
 *  parameter is used to control the tabbing of the listing: an initial
 *  value of 1 will produce a listing with indented top-level contents,
 *  which is normally what you want, but 0 might also be desirable
 *  desirable in certain cases.
 */
nestlistcont: function(obj, depth)
{
    /* 
     *   use the general-purpose contents lister to show a "tall" listing,
     *   recursing into contents of the items in the list 
     */
    listcontgen(obj, LCG_TALL | LCG_RECURSE | LCG_CHECKLIST, depth);
}

/*
 *   contentsListable: are the contents of the given object listable in an
 *   inventory or description list?  Returns true if the object's contents
 *   are visible and it's not a "quiet" container/surface.  
 */
contentsListable: function(obj)
{
    /* check to see if it's a surface or container */
    if (obj.issurface)
    {
        /* surface - list the contents unless it's a "quiet" surface */
        return (not obj.isqsurface);
    }
    else
    {
        /* 
         *   container - list the contents if the container makes its
         *   contents visible and it's not a "quiet" container 
         */
        return (obj.contentsVisible and not obj.isqcontainer);
    }
}

/*
 *   showcontcont:  list the contents of the object, plus the contents of
 *   an fixeditem's contained by the object.  A complete sentence is shown.
 *   This is an internal routine used by listcontcont and listfixedcontcont.
 */
showcontcont: function(obj)
{
    /* show obj's direct contents, if it has any and they're listable */
    if (itemcnt(obj.contents) != 0 && contentsListable(obj))
    {
        if (obj.issurface)
        {
            "Sitting on "; obj.thedesc;" is "; listcont(obj);
            ". ";
        }
        else
        {
            caps();
            obj.thedesc; " seem";
            if (!obj.isThem) "s";
            " to contain ";
            listcont(obj);
            ". ";
        }
    }

    /* 
     *   show the contents of the fixed contents if obj's contents are
     *   themselves listable 
     */
    if (contentsListable(obj))
        listfixedcontcont(obj);
}

/*
 *  listfixedcontcont: function(obj)
 *
 *  List the contents of the contents of any fixeditem objects
 *  in the contents list of the object obj.  This routine
 *  makes sure that all objects that can be taken are listed somewhere
 *  in a room's description.  This routine recurses down the contents
 *  tree, following each branch until either something has been listed
 *  or the branch ends without anything being listable.  This routine
 *  displays a complete sentence, so no introductory or closing text
 *  is needed.
 */
listfixedcontcont: function(obj)
{
    local list, i, tot, thisobj;

    list := obj.contents;
    tot := length(list);
    i := 1;
    while (i <= tot)
    {
        thisobj := list[i];
        if (thisobj.isfixed and thisobj.contentsVisible and
             not thisobj.isqcontainer)
            showcontcont(thisobj);
        i := i + 1;
    }
}

/*
 *  listcontcont: function(obj)
 *
 *  This function lists the contents of the contents of an object.
 *  It displays full sentences, so no introductory or closing text
 *  is required.  Any item in the contents list of the object
 *  obj whose contentsVisible property is true has
 *  its contents listed.  An Object whose isqcontainer or
 *  isqsurface property is true will not have its
 *  contents listed.
 */
listcontcont: function(obj)
{
    local list, i, tot;
    
    list := obj.contents;
    tot := length(list);
    i := 1;
    while (i <= tot)
    {
        showcontcont(list[i]);
        i := i + 1;
    }
}

/*
 *  scoreStatus: function(points, turns)
 *
 *  This function updates the score on the status line.  This implementation
 *  simply calls the built-in function setscore() with the same information.
 *  The call to setscore() has been isolated in this function to make it
 *  easier to replace with a customized version; to replace the status line
 *  score display, simply replace this routine.
 */
scoreStatus: function(points, turns)
{
    /* get the formatted score string, and set the score to that value */
    setscore(scoreFormat(points, turns));
}

/*
 *  scoreValue: function(points, turns)
 *
 *  This function returns the formatted display string to show in the right
 *  half of the status line, where the score and turn count are normally
 *  displayed.  If you wantd to display a different type of information in
 *  the right half of the status line, you can override this function to
 *  return the string you wish to display.
 */
scoreFormat: function(points, turns)
{
    /* return a string with the standard points/turns display */
    return cvtstr(points) + '/' + cvtstr(turns);
}

/*
 *  turncount: function(parm)
 *
 *  This function can be used as a daemon (normally set up in the init
 *  function) to update the turn counter after each turn.  This routine
 *  increments global.turnsofar, and then calls setscore to
 *  update the status line with the new turn count.
 */
turncount: function(parm)
{
    incturn();
    global.turnsofar := global.turnsofar + 1;
    scoreStatus(global.score, global.turnsofar);
}

/*
 *  addweight: function(list)
 *
 *  Adds the weights of the objects in list and returns the sum.
 *  The weight of an object is given by its weight property.  This
 *  routine includes the weights of all of the contents of each object,
 *  and the weights of their contents, and so forth.
 */
addweight: function(l)
{
    local tot, i, c, totweight;

    tot := length(l);
    i := 1;
    totweight := 0;
    while (i <= tot)
    {
        c := l[i];
        totweight := totweight + c.weight;
        if (length(c.contents))
            totweight := totweight + addweight(c.contents);
        i := i + 1;
    }
    return totweight;
}

/*
 *  addbulk: function(list)
 *
 *  This function returns the sum of the bulks (given by the bulk
 *  property) of each object in list.  The value returned includes
 *  only the bulk of each object in the list, and not of the contents
 *  of the objects, as it is assumed that an object does not change in
 *  size when something is put inside it.  You can easily change this
 *  assumption for special objects (such as a bag that stretches as
 *  things are put inside) by writing an appropriate bulk method
 *  for that object.
 */
addbulk: function(list)
{
    local i, tot, totbulk, rem, cur;

    tot := length(list);
    i := 1;
    totbulk := 0;
    while(i <= tot)
    {
        cur := list[i];
        if (not cur.isworn)
            totbulk := totbulk + cur.bulk;
        i := i + 1;
    }
    return totbulk;
}

/*
 *  incscore: function(amount)
 *
 *  Adds amount to the total score, and updates the status line
 *  to reflect the new score.  The total score is kept in global.score.
 *  Always use this routine rather than changing global.score
 *  directly, since this routine ensures that the status line is
 *  updated with the new value.
 */
incscore: function(amount)
{
    global.score := global.score + amount;
    scoreStatus(global.score, global.turnsofar);
}

/*
 *  initSearch: function
 *
 *  Initializes the containers of objects with a searchLoc, underLoc,
 *  and behindLoc by setting up searchCont, underCont, and
 *  behindCont lists, respectively.  You should call this function once in
 *  your preinit (or init, if you prefer) function to ensure that
 *  the underable, behindable, and searchable objects are set up correctly.
 *  
 *  As a bonus, we take this opportunity to initialize global.floatingList
 *  with a list of all objects of class floatingItem.  It is necessary to
 *  initialize this list, so that validDoList and validIoList include objects
 *  with variable location properties.  Note that, for this to work,
 *  all objects with variable location properties must be declared
 *  to be of class floatingItem.
 */
initSearch: function
{
    local o;
    
    o := firstobj(hiddenItem);
    while (o <> nil)
    {
        if (o.searchLoc)
            o.searchLoc.searchCont := o.searchLoc.searchCont + o;
        else if (o.underLoc)
            o.underLoc.underCont := o.underLoc.underCont + o;
        else if (o.behindLoc)
            o.behindLoc.behindCont := o.behindLoc.behindCont + o;
        o := nextobj(o, hiddenItem);
    }
    
    global.floatingList := [];
    for (o := firstobj(floatingItem) ; o ; o := nextobj(o, floatingItem))
        global.floatingList += o;
}

/*
 *  reachableList: function
 *
 *  Returns a list of all the objects reachable from a given object.
 *  That is, if the object is open or is not an openable, it returns the
 *  contents of the object, plus the reachableList result of each object;
 *  if the object is closed, it returns an empty list.
 */
reachableList: function(obj)
{
    local ret := [];
    local i, lst, len;
    
    if (not isclass(obj, openable)
        or (isclass(obj, openable) and obj.isopen))
    {
        lst := obj.contents;
        len := length(lst);
        ret += lst;
        for (i := 1 ; i <= len ; ++i)
            ret += reachableList(lst[i]);
    }

    return ret;
}

/*
 *  visibleList: function
 *
 *  This function is similar to reachableList, but returns the
 *  list of objects visible within a given object.
 */
visibleList: function(obj, actor)
{
    local ret := [];
    local i, lst, len;
    
    /* don't look in "nil" objects */
    if (obj = nil)
        return ret;

    /* 
     *   get the object's contents if it's not openable at all (in which
     *   case we assume its contents are visible unconditionally), or if
     *   it's openable and it's currently open, or if it's openable and
     *   its contents are explicitly visible (which will be the case for
     *   transparent openables whether they're opened or closed) 
     */
    if (not isclass(obj, openable)
        or (isclass(obj, openable) and obj.isopen)
        or obj.contentsVisible)
    {
        lst := obj.contents;
        len := length(lst);
        ret += lst;
        for (i := 1 ; i <= len ; ++i)
        {
            /* recurse into this object, but not into the current actor */
            if (lst[i] <> actor)
                ret += visibleList(lst[i], actor);
        }

        /* 
         *   since the "Me" object never shows up in room.contents, if Me is
         *   located in obj, and actor <> Me, add Me.contents to the list 
         */
        if (actor <> parserGetMe() and parserGetMe().location = obj)
            ret += visibleList(parserGetMe(), actor);
    }
    
    return ret;
}

/*
 *  numbered_cleanup: function
 *
 *  This function is used as a fuse to delete objects created by the
 *  numberedObject class in reponse to calls to its newNumbered
 *  method.  Whenever that method creates a new object, it sets up a fuse
 *  call to this function to delete the object at the end of the turn in
 *  which it created the object.
 */
numbered_cleanup: function(obj)
{
    delete obj;
}

/*
 *  mainRestore: function
 *
 *  Restore a saved game.  Returns true on success, nil on failure.  If
 *  the game is restored successfully, we'll update the status line and
 *  show the full description of the current location.  The restore command
 *  uses this function; this can also be used by initRestore (which must be
 *  separately defined by the game, since it's not defined in this file).
 */
mainRestore: function(fname)
{
    /* try restoring the game */
    switch(restore(fname))
    {
    case RESTORE_SUCCESS:
        /* update the status line */
        scoreStatus(global.score, global.turnsofar);

        /* tell the user we succeeded, and show the location */
        "Restored.\b";
        parserGetMe().location.lookAround(true);
        
        /* success */
        return true;

    case RESTORE_FILE_NOT_FOUND:
        "The saved position file could not be opened. ";
        return nil;

    case RESTORE_NOT_SAVE_FILE:
        "This file does not contain a saved game position. ";
        return nil;

    case RESTORE_BAD_FMT_VSN:
        "This file was saved by another version of TADS that is
        not compatible with the current version. ";
        return nil;

    case RESTORE_BAD_GAME_VSN:
        "This file was saved by a different game, or a different
        version of this game, and cannot be restored with this game. ";
        return nil;

    case RESTORE_READ_ERROR:
        "An error occurred reading the saved position file; the file
        might be corrupted.  The game might have been partially restored,
        which could make it impossible to continue playing; if the game
        does not seem to be working properly, you should quit the game
        and start over. ";
        return nil;

    case RESTORE_NO_PARAM_FILE:
        /* 
         *   ignore the error, since the caller was only asking, and
         *   return nil 
         */
        return nil;

    default:
        "Restore failed. ";
        return nil;
    }
}

/*
 *  switchPlayer: function
 *
 *  Switch the player character to a new actor.  This function uses the
 *  parserSetMe() built-in function to change the parser's internal
 *  record of the player character object, and in addition performs some
 *  necessary modifications to the outgoing and incoming player character
 *  objects.  First, because the player character object is by convention
 *  never part of its room's contents list, we add the outgoing actor to
 *  its location's contents list, and remove the incoming actor from its
 *  location's contents list.  Second, we remove the vocabulary words
 *  "me" and "myself" from the outgoing object, and add them to the
 *  incoming object.
 */
switchPlayer: function(newPlayer)
{
    /* if this is the same as the current player character, do nothing */
    if (parserGetMe() = newPlayer)
        return;

    /* 
     *   add the outgoing player character to its location's contents --
     *   it's going to be an ordinary actor from now on, so it should be
     *   in its location's contents list 
     */
    if (parserGetMe() != nil && parserGetMe().location != nil)
        parserGetMe().location.contents += parserGetMe();

    /* 
     *   remove the incoming player character from its location's contents
     *   -- by convention, the active player character is never in its
     *   location's contents list 
     */
    if (newPlayer != nil && newPlayer.location != nil)
        newPlayer.location.contents -= newPlayer;

    /* 
     *   remove "me" and "myself" from the old object's vocabulary -- it's
     *   no longer "me" from the player's perspective 
     */
    delword(parserGetMe(), &noun, 'me');
    delword(parserGetMe(), &noun, 'myself');

    /* establish the new player character object in the parser */
    parserSetMe(newPlayer);

    /* add "me" and "myself" to the new player character's vocabulary */
    addword(newPlayer, &noun, 'me');
    addword(newPlayer, &noun, 'myself');
}

/*
 *  numberedObject: object
 *
 *  This class can be added to a class list for an object to allow it to
 *  be used as a generic numbered object.  You can create a single object
 *  with this class, and then the player can refer to that object with
 *  any number.  For example, you can create a single "button" object
 *  that the player can refer to with "button 100" or "button 1000"
 *  or any other number.  If you want to limit the range of acceptable
 *  numbers, override the num_is_valid method so that it displays
 *  an appropriate error message and returns nil for invalid numbers.
 *  If you want to use a separate object to handle references to the object
 *  with a plural ("look at buttons"), override newNumberedPlural to
 *  return the object to handle these references; by default, the original
 *  object is used to handle plurals.
 */

class numberedObject: object
    adjective = '#'
    anyvalue(n) = { return n; }
    clean_up = { delete self; }
    newNumberedPlural(a, v) = { return self; }
    newNumbered(a, v, n) =
    {
        local obj;
        
        if (n = nil)
            return self.newNumberedPlural(a, v);
        if (not self.num_is_valid(n))
            return nil;
        obj := new self;
        obj.value := n;
        setfuse(numbered_cleanup, 0, obj);
        return obj;
    }
    num_is_valid(n) = { return true; }
    dobjGen(a, v, i, p) =
    {
        if (self.value = nil)
        {
            "%You%'ll have to be more specific about which one %you% mean.";
            exit;
        }
    }
    iobjGen(a, v, d, p) = { self.dobjGen(a, v, d, p); }
;


/*
 *  nestedroom: room
 *
 *  A special kind of room that is inside another room; chairs and
 *  some types of vehicles, such as inflatable rafts, fall into this
 *  category.  Note that a room can be within another room without
 *  being a nestedroom, simply by setting its location property
 *  to another room.  The nestedroom is different from an ordinary
 *  room, though, in that it's an "open" room; that is, when inside it,
 *  the actor is still really inside the enclosing room for purposes of
 *  descriptions.  Hence, the player sees "Laboratory, in the chair."
 *  In addition, a nestedroom is an object in its own right,
 *  visible to the player; for example, a chair is an object in a
 *  room in addition to being a room itself.  The statusPrep
 *  property displays the preposition in the status description; by
 *  default, it will be "in," but some subclasses and instances
 *  will want to change this to a more appropriate preposition.
 *  outOfPrep is used to report what happens when the player
 *  gets out of the object:  it should be "out of" or "off of" as
 *  appropriate to this object.
 */
class nestedroom: room
    statusPrep = "in"
    outOfPrep = "out of"
    islit =
    {
        if (self.location)
            return self.location.islit;
        return nil;
    }
    statusRoot =
    {
        "<<self.location.sdesc>>, <<self.statusPrep>> <<self.thedesc>>";
    }
    lookAround(verbosity) =
    {
        self.dispBeginSdesc;
        self.statusRoot;
        self.dispEndSdesc;
        
        self.location.nrmLkAround(verbosity);
    }
    roomDrop(obj) =
    {
        if (self.location = nil or self.isdroploc)
            pass roomDrop;
        else
            self.location.roomDrop(obj);
    }

    /*
     *   canReachContents - this flag indicates whether or not the player
     *   can reach the contents of objects in the 'reachable' list from
     *   this nested room.  This is true by default, which means that the
     *   player can reach not only the items directly in the 'reachable'
     *   list, but also the objects within those objects.  If this is nil,
     *   it means that the player can only reach the objects actually
     *   listed in the 'reachable' list, and not any objects within those
     *   objects.
     *   
     *   Setting canReachContents to nil is probably undesirable in most
     *   cases.  This setting is provided mostly for compatibility with
     *   past versions of adv.t, which behaved as though canReachContents
     *   was true for nested rooms *except* for chairItem objects, which
     *   acted as though canReachContents was nil.  If you want consistent
     *   behavior in all nested rooms including chairs, simply set
     *   chairItem.canReachContents to true.  
     */
    canReachContents = true

    /*
     *   If canReachContents is nil, enforce the limitation 
     */
    roomAction(actor, v, dobj, prep, io) =
    {
        /* enforce !canReachContents, except for 'look at' */
        if (dobj != nil && !self.canReachContents && v != inspectVerb)
            checkReach(self, actor, v, dobj);

        /* enforce !canReachContents, except for 'ask/tell about' */
        if (io != nil && !self.canReachContents
             && v != askVerb && v != tellVerb)
            checkReach(self, actor, v, io);

        /* inherit the default behavior */
        inherited.roomAction(actor, v, dobj, prep, io);
    }

    /*
     *   This routine is called when the actor is in this nested room, and
     *   tries to manipulate something that is visible but not reachable.
     *   This generally means that the object is in the enclosing room (or
     *   within an object in the enclosing room), but is not in our own
     *   "reachable" list, making the object visible but not touchable.
     *   We'll just say so.  
     */
    cantReachRoom(otherRoom) =
    {
        "%You% can't reach that from <<self.thedesc>>. ";
    }
;

/*
 *  chairitem: fixeditem, nestedroom, surface
 *
 *  Acts like a chair:  actors can sit on the object.  While sitting
 *  on the object, an actor can't go anywhere until standing up, and
 *  can only reach objects that are on the chair and in the chair's
 *  reachable list.  By default, nothing is in the reachable
 *  list.  Note that there is no real distinction made between chairs
 *  and beds, so you can sit or lie on either; the only difference is
 *  the message displayed describing the situation.
 */
class chairitem: fixeditem, nestedroom, surface
    reachable = ([] + self) // list of all containers reachable from here;
                            //  normally, you can only reach carried items
                            //  from a chair, but this makes special allowances
    ischair = true          // it is a chair by default; for beds or other
                            //  things you lie down on, make it false

    /*
     *   For compatibility with past versions of adv.t, we do not allow
     *   the player to reach objects that are inside objects that are in
     *   the reachable list.  This behavior probably doesn't make a lot of
     *   sense in most cases, and isn't consistent with other types of
     *   nested rooms, so many games will probably want to modify
     *   chairItem and set canReachContents = true.  However, since it
     *   worked this way in the past, the default is still nil, so that
     *   any games that (intentionally or otherwise) depended on this
     *   behavior will continue to work the same way they used to.  
     */
    canReachContents = nil

    outOfPrep = "out of"
    enterRoom(actor) = {}
    noexit =
    {
        "%You're% not going anywhere until %you%
        get%s% <<outOfPrep>> <<thedesc>>. ";
        return nil;
    }
    verDoBoard(actor) = { self.verDoSiton(actor); }
    doBoard(actor) = { self.doSiton(actor); }
    verDoSiton(actor) =
    {
        if (actor.location = self)
        {
            "%You're% already on "; self.thedesc; "! ";
        }
    }
    doSiton(actor) =
    {
        "Okay, %you're% now sitting on "; self.thedesc; ". ";
        actor.travelTo(self);
    }
    verDoLieon(actor) =
    {
        self.verDoSiton(actor);
    }
    doLieon(actor) =
    {
        self.doSiton(actor);
    }
;

/*
 *  beditem: chairitem
 *
 *  This object is the same as a chairitem, except that the player
 *  is described as lying on, rather than sitting in, the object.
 */
class beditem: chairitem
    ischair = nil
    isbed = true
    sdesc = "bed"
    statusPrep = "on"
    outOfPrep = "out of"
    doLieon(actor) =
    {
        "Okay, %you're% now lying on <<self.thedesc>>.";
        actor.travelTo(self);
    }
;
    
/*
 *  floatingItem: object
 *
 *  This class doesn't do anything apart from mark an object as having a
 *  variable location property.  It is necessary to mark all such
 *  items by making them a member of this class, so that the objects are
 *  added to global.floatingList, which is necessary so that floating
 *  objects are included in validDoList and validIoList values (see
 *  the deepverb class for a description of these methods).
 */
class floatingItem: object
;

/*
 *  thing: object
 *
 *  The basic class for objects in a game.  The property contents
 *  is a list that specifies what is in the object; this property is
 *  automatically set up by the system after the game is compiled to
 *  contain a list of all objects that have this object as their
 *  location property.  The contents property is kept
 *  consistent with the location properties of referenced objects
 *  by the moveInto method; always use moveInto rather than
 *  directly setting a location property for this reason.  The
 *  adesc method displays the name of the object with an indefinite
 *  article; the default is to display "a" followed by the sdesc,
 *  but objects that need a different indefinite article (such as "an"
 *  or "some") should override this method.  Likewise, thedesc
 *  displays the name with a definite article; by default, thedesc
 *  displays "the" followed by the object's sdesc.  The sdesc
 *  simply displays the object's name ("short description") without
 *  any articles.  The ldesc is the long description, normally
 *  displayed when the object is examined by the player; by default,
 *  the ldesc displays "It looks like an ordinary sdesc."
 *  The isIn(object) method returns true if the
 *  object's location is the specified object or the object's
 *  location is an object whose contentsVisible property is
 *  true and that object's isIn(object) method is
 *  true.  Note that if isIn is true, it doesn't
 *  necessarily mean the object is reachable, because isIn is
 *  true if the object is merely visible within the location.
 *  The moveInto(object) method moves the object to be inside
 *  the specified object.  To make an object disappear, move it
 *  into nil.
 */
class thing: object
    weight = 0
    statusPrep = "on"       // give everything a default status preposition
    bulk = 1
    isListed = true         // shows up in room/inventory listings
    contents = []           // set up automatically by system - do not set
    verGrab(obj) = {}
    Grab(obj) = {}
    adesc =
    {
        "a "; self.sdesc;   // default is "a <name>"; "self" is current object
    }
    pluraldesc =            // default is to add "s" to the sdesc
    {
        self.sdesc;
        if (not self.isThem)
            "s";
    }
    thedesc =
    {
        "the "; self.sdesc; // default is "the <name>"
    }
    itnomdesc =
    {
        /* 
         *   display "it" in the nominative case: "it" if this is a
         *   singular object, "they" otherwise 
         */
        if (self.isThem)
            "they";
        else
            "it";
    }
    itobjdesc =
    {
        /* 
         *   display "it" in the objective case: "it" for singular, "them"
         *   otherwise 
         */
        if (self.isThem)
            "them";
        else
            "it";
    }
    itisdesc =
    {
        /* display "it's" if this is singular, "they're" otherwise */
        if (self.isThem)
            "they're";
        else
            "it's";
    }
    doesdesc =
    {
        /* display "does" if singular, "do" otherwise (it does, they do) */
        if (self.isThem)
            "do";
        else
            "does";
    }
    isdesc =
    {
        /* display "is" if this is singular, "are" otherwise */
        if (self.isThem)
            "are";
        else
            "is";
    }
    isntdesc =
    {
        /* display "isn't" if this is singular, "aren't" otherwise */
        if (self.isThem)
            "aren't";
        else
            "isn't";
    }
    itselfdesc =
    {
        /* display "itself" if this is singular, "themselves" otherwise */
        if (self.isThem)
            "themselves";
        else
            "itself";
    }
    thatdesc =
    {
        /* display "that" if this is singular, "them" otherwise */
        if (self.isThem)
            "those";
        else
            "that";
    }
    multisdesc = { self.sdesc; }
    ldesc =
    {
        if (self.isThem)
            "\^<<self.itnomdesc>> look like ordinary <<self.sdesc>> to %me%.";
        else
            "\^<<self.itnomdesc>> looks like an
            ordinary <<self.sdesc>> to %me%.";
    }
    readdesc = { "%You% can't read "; self.adesc; ". "; }
    actorAction(v, d, p, i) =
    {
        "%You% %have% lost %your% mind. ";
        exit;
    }
    contentsVisible = { return true; }
    contentsReachable = { return true; }
    isIn(obj) =
    {
        local myloc;

        myloc := self.location;
        if (myloc)
        {
            if (myloc = obj)
                return true;
            if (myloc.contentsVisible)
                return myloc.isIn(obj);
        }
        return nil;
    }
    moveInto(obj) =
    {
        local loc;

        /*
         *   For the object containing me, and its container, and so forth,
         *   tell it via a Grab message that I'm going away.
         */
        loc := self.location;
        while (loc)
        {
            loc.Grab(self);
            loc := loc.location;
        }
        
        if (self.location)
            self.location.contents := self.location.contents - self;
        self.location := obj;
        if (obj)
            obj.contents := obj.contents + self;
    }
    verDoSave(actor) =
    {
        "Please specify the name of the game to save in double quotes,
        for example, SAVE \"GAME1\". ";
    }
    verDoRestore(actor) =
    {
        "Please specify the name of the game to restore in double quotes,
        for example, RESTORE \"GAME1\". ";
    }
    verDoScript(actor) =
    {
        "You should type the name of a file to write the transcript to
        in quotes, for example, SCRIPT \"LOG1\". ";
    }
    verDoSay(actor) =
    {
        "You should put what you want said in double quotes; for example,
        SAY \"HELLO\". ";
    }
    verDoPush(actor) =
    {
        "Pushing <<self.thedesc>> doesn't do anything. ";
    }

    verDoAttachTo(actor, iobj) = { "There's no obvious way to do that."; }
    verIoAttachTo(actor) = { "There's no obvious way to do that."; }

    verDoDetach(actor) = { "There's no obvious way to do that."; }
    verDoDetachFrom(actor, iobj) = { "There's no obvious way to do that."; }
    verIoDetachFrom(actor) = { "There's no obvious way to do that."; }

    verDoWear(actor) =
    {
        "%You% can't wear "; self.thedesc; ". ";
    }
    verDoTake(actor) =
    {
        if (self.location = actor)
        {
            "%You% already %have% "; self.thedesc; "! ";
        }
        else
            self.verifyRemove(actor);
    }
    verifyRemove(actor) =
    {
        /*
         *   Check with each container to make sure that the container
         *   doesn't object to the object's removal.
         */
        local loc;

        loc := self.location;
        while (loc)
        {
            if (loc <> actor)
                loc.verGrab(self);
            loc := loc.location;
        }
    }
    isVisible(vantage) =
    {
        local loc;

        loc := self.location;
        if (loc = nil)
            return nil;

        /*
         *   if the vantage is inside me, and my contents are visible,
         *   I'm visible 
         */
        if (vantage.location = self and self.contentsVisible)
            return true;
        
        /* if I'm in the vantage, I'm visible */
        if (loc = vantage)
            return true;

        /*
         *   if its location's contents are visible, and its location is
         *   itself visible, it's visible
         */
        if (loc.contentsVisible and loc.isVisible(vantage))
            return true;

        /*
         *   If the vantage has a location, and the vantage's location's
         *   contents are visible (if you can see me I can see you), and
         *   the object is visible from the vantage's location, the object
         *   is visible
         */
        if (vantage.location <> nil and vantage.location.contentsVisible and
            self.isVisible(vantage.location))
            return true;

        /* all tests failed:  it's not visible */
        return nil;
    }

    /*
     *   This routine is called when the actor tries to manipulate this
     *   object, and the object is visible to the actor but can't be
     *   touched.
     *   
     *   If this object is inside something which doesn't block access to
     *   its contents (such as a closed container), we'll let the
     *   container handle it; this can bubble up to the enclosing room if
     *   all enclosing containers are open.
     *   
     *   If this object is inside a closed container, we'll point out that
     *   the container must be opened before the object can be
     *   manipulated.  This usually means that the container is closed but
     *   transparent, so its contents can be seen but not touched.  
     */
    cantReach(actor) =
    {
        if (not actor.isactor)
        {
            "\^<<actor.thedesc>> <<actor.doesdesc>> not respond.";
            return;
        }
        if (not isclass(actor, movableActor))
        {
            "There's no point in talking to <<actor.sdesc>>.";
            return;
        }
        
        if (self.location = nil)
        {
            if (actor.location.location)
                "%You% can't reach <<self.thatdesc>> from <<
                 actor.location.thedesc >>. ";
            else
                "%You% can't reach <<self.thatdesc>>.";
            return;
        }
        if (not self.location.isopenable or self.location.isopen)
            self.location.cantReach(actor);
        else
            "%You%'ll have to open << self.location.thedesc >> first. ";
    }

    /*
     *   This routine is called when the actor is (directly) within this
     *   object, and is trying to reach something in the given room, but
     *   cannot.  This should generate an appropriate message explaining
     *   why objects in the given room are visible but not reachable.  By
     *   default, we'll just say that the object is not reachable; some
     *   rooms may wish to explain more fully why this is the case. 
     */
    cantReachRoom(otherRoom) =
    {
        "%You% can't reach that from here. ";
    }

    isReachable(actor) =
    {
        local loc;

        /* make sure the actor's location has a reachable list */
        if (actor.location = nil or actor.location.reachable = nil)
            return nil;

        /* if the object is in the room's 'reachable' list, it's reachable */
        if (find(actor.location.reachable, self) <> nil)
            return true;

        /*
         *   If the object's container's contents are reachable, and the
         *   container is reachable, the object is reachable.
         */
        loc := self.location;
        if (loc = nil)
            return nil;
        if (loc = actor or loc = actor.location)
            return true;
        if (loc.contentsReachable)
            return loc.isReachable(actor);
        return nil;
    }
    doTake(actor) =
    {
        local totbulk, totweight;

        totbulk := addbulk(actor.contents) + self.bulk;
        totweight := addweight(actor.contents);
        if (not actor.isCarrying(self))
            totweight := totweight + self.weight + addweight(self.contents);

        if (totweight > actor.maxweight)
            "%Your% load is too heavy. ";
        else if (totbulk > actor.maxbulk)
            "%You've% already got %your% hands full. ";
        else
        {
            self.moveInto(actor);
            "Taken. ";
        }
    }
    verDoDrop(actor) =
    {
        if (not actor.isCarrying(self))
        {
            "%You're% not carrying "; self.thedesc; "! ";
        }
        else self.verifyRemove(actor);
    }
    doDrop(actor) =
    {
        actor.location.roomDrop(self);
    }
    verDoUnwear(actor) =
    {
        "%You're% not wearing "; self.thedesc; "! ";
    }
    verIoPutIn(actor) =
    {
        "%You% can't put anything into "; self.thedesc; ". ";
    }
    circularMessage(io) =
    {
        local cont, prep;

        /* set prep to 'on' if io.location is a surface, 'in' otherwise */
        prep := (io.location.issurface ? 'on' : 'in');

        /* tell them about the problem */
        "%You% can't put <<thedesc>> <<prep>> <<io.thedesc>>, because <<
            io.thedesc>> <<io.isdesc>> <<
            io.location = self ? "already" : "">> <<prep>> <<
            io.location.thedesc>>";
        for (cont := io.location ; cont <> self ; cont := cont.location)
        {
            ", which <<cont.isdesc>> <<
                cont.location = self ? "already" : "">> <<prep>> <<
                cont.location.thedesc>>";
        }
        ". ";
    }
    verDoPutIn(actor, io) =
    {
        if (io = nil)
            return;

        if (self.location = io)
        {
            caps(); self.thedesc; " is already in "; io.thedesc; "! ";
        }
        else if (io = self)
        {
            "%You% can't put "; self.thedesc; " in <<self.itselfdesc>>! ";
        }
        else if (io.isIn(self))
            self.circularMessage(io);
        else
            self.verifyRemove(actor);
    }
    doPutIn(actor, io) =
    {
        self.moveInto(io);
        "Done. ";
    }
    verIoPutOn(actor) =
    {
        "There's no good surface on "; self.thedesc; ". ";
    }
    verDoPutOn(actor, io) =
    {
        if (io = nil)
            return;

        if (self.location = io)
        {
            caps(); self.thedesc; " is already on "; io.thedesc; "! ";
        }
        else if (io = self)
        {
            "%You% can't put "; self.thedesc; " on <<self.itselfdesc>>! ";
        }
        else if (io.isIn(self))
            self.circularMessage(io);
        else
            self.verifyRemove(actor);
    }
    doPutOn(actor, io) =
    {
        self.moveInto(io);
        "Done. ";
    }
    verIoTakeOut(actor) = {}
    ioTakeOut(actor, dobj) =
    {
        dobj.doTakeOut(actor, self);
    }
    verDoTakeOut(actor, io) =
    {
        if (io <> nil and not self.isIn(io))
        {
            caps(); self.thedesc; " "; self.isntdesc;
            " in "; io.thedesc; ". ";
        }
        else
            self.verDoTake(actor);     /* ensure object can be taken at all */
    }
    doTakeOut(actor, io) =
    {
        self.doTake(actor);
    }
    verIoTakeOff(actor) = {}
    ioTakeOff(actor, dobj) =
    {
        dobj.doTakeOff(actor, self);
    }
    verDoTakeOff(actor, io) =
    {
        if (io <> nil and not self.isIn(io))
        {
            caps(); self.thedesc; " "; self.isntdesc;
            " on "; io.thedesc; "! ";
        }
        self.verDoTake(actor);         /* ensure object can be taken at all */
    }
    doTakeOff(actor, io) =
    {
        self.doTake(actor);
    }
    verIoPlugIn(actor) =
    {
        "%You% can't plug anything into "; self.thedesc; ". ";
    }
    verDoPlugIn(actor, io) =
    {
        "%You% can't plug "; self.thedesc; " into anything. ";
    }
    verIoUnplugFrom(actor) =
    {
        "It's not plugged into "; self.thedesc; ". ";
    }
    verDoUnplugFrom(actor, io) =
    {
        if (io <> nil)
            { "\^<<self.itisdesc>> not plugged into "; io.thedesc; ". "; }
    }
    verDoLookin(actor) =
    {
        "There's nothing in "; self.thedesc; ". ";
    }
    thrudesc = { "%You% can't see much through << thedesc >>.\n"; }
    verDoLookthru(actor) =
    {
        "%You% can't see anything through "; self.thedesc; ". ";
    }
    verDoLookunder(actor) =
    {
        "There's nothing under "; self.thedesc; ". ";
    }
    verDoInspect(actor) = {}
    doInspect(actor) =
    {
        self.ldesc;
    }
    verDoRead(actor) =
    {
        "I don't know how to read "; self.thedesc; ". ";
    }
    verDoLookbehind(actor) =
    {
        "There's nothing behind "; self.thedesc; ". ";
    }
    verDoTurn(actor) =
    {
        "Turning <<self.thedesc>> doesn't have any effect. ";
    }
    verDoTurnWith(actor, io) =
    {
        "Turning <<self.thedesc>> doesn't have any effect. ";
    }
    verDoTurnTo(actor, io) =
    {
        "Turning <<self.thedesc>> doesn't have any effect. ";
    }
    verIoTurnTo(actor) =
    {
        "I don't know how to do that. ";
    }
    verDoTurnon(actor) =
    {
        "I don't know how to turn "; self.thedesc; " on. ";
    }
    verDoTurnoff(actor) =
    {
        "I don't know how to turn "; self.thedesc; " off. ";
    }

    verDoScrew(actor) = { "%You% see%s% no way to do that."; }
    verDoScrewWith(actor, iobj) = { "%You% see%s% no way to do that."; }
    verIoScrewWith(actor) = { "%You% can't use <<self.thedesc>> like that."; }

    verDoUnscrew(actor) = { "%You% see%s% no way to do that."; }
    verDoUnscrewWith(actor, iobj) = { "%You% see%s% no way to do that."; }
    verIoUnscrewWith(actor) =
        { "%You% can't use <<self.thedesc>> like that."; }

    verIoAskAbout(actor) = {}
    ioAskAbout(actor, dobj) =
    {
        dobj.doAskAbout(actor, self);
    }
    verDoAskAbout(actor, io) =
    {
        "Surely, %you% can't think "; self.thedesc; " knows anything
        about it! ";
    }
    verIoTellAbout(actor) = {}
    ioTellAbout(actor, dobj) =
    {
        dobj.doTellAbout(actor, self);
    }
    verDoTellAbout(actor, io) =
    {
        "It doesn't look as though <<self.thedesc>> <<
          self.isdesc>> interested. ";
    }
    verDoUnboard(actor) =
    {
        if (actor.location <> self)
        {
            "%You're% not <<self.statusPrep>> <<self.thedesc>>! ";
        }
        else if (self.location = nil)
        {
            "%You% can't leave <<self.thedesc>>! ";
        }
    }
    doUnboard(actor) =
    {
        if (self.fastenitem)
        {
            "%You%'ll have to unfasten <<actor.location.fastenitem.thedesc
            >> first. ";
        }
        else
        {
            "Okay, %you're% no longer <<self.statusPrep>> <<self.thedesc>>. ";
            self.leaveRoom(actor);
            actor.moveInto(self.location);
        }
    }
    verDoAttackWith(actor, io) =
    {
        "Attacking <<self.thedesc>> doesn't appear productive. ";
    }
    verIoAttackWith(actor) =
    {
        "It's not very effective to attack with <<self.thedesc>>. ";
    }
    verDoEat(actor) =
    {
        "\^<<self.thedesc>> <<self.doesdesc>>n't appear appetizing. ";
    }
    verDoDrink(actor) =
    {
        "\^<<self.thedesc>> <<self.doesdesc>>n't appear appetizing. ";
    }
    verDoGiveTo(actor, io) =
    {
        if (not actor.isCarrying(self))
        {
            "%You're% not carrying "; self.thedesc; ". ";
        }
        else self.verifyRemove(actor);
    }
    doGiveTo(actor, io) =
    {
        self.moveInto(io);
        "Done. ";
    }
    verDoPull(actor) =
    {
        "Pulling "; self.thedesc; " doesn't have any effect. ";
    }
    verDoThrowAt(actor, io) =
    {
        if (not actor.isCarrying(self))
        {
            "%You're% not carrying "; self.thedesc; ". ";
        }
        else self.verifyRemove(actor);
    }
    doThrowAt(actor, io) =
    {
        local loc;
        
        "%You% miss%es%. ";

        /* move it to the actor's location's thrown object destination */
        loc := actor.location;
        if (loc != nil)
            loc := loc.throwHitDest;

        /* move it to the location we figured out */
        self.moveInto(loc);
    }

    /* 
     *   The destination location of a thrown object when an actor is
     *   inside me.  By default, this just returns 'self'.  In some cases,
     *   special nested rooms might want to make the thrown object land
     *   somewhere different.  
     */
    throwHitDest = { return self; }
    
    verIoThrowAt(actor) =
    {
        if (actor.isCarrying(self))
        {
            "%You% could at least drop "; self.thedesc; " first. ";
        }
    }
    ioThrowAt(actor, dobj) =
    {
        dobj.doThrowAt(actor, self);
    }
    verDoThrowTo(actor, io) =
    {
        if (not actor.isCarrying(self))
        {
            "%You're% not carrying "; self.thedesc; ". ";
        }
        else
            self.verifyRemove(actor);
    }
    doThrowTo(actor, io) =
    {
        "%You% miss%es%. ";
        self.moveInto(actor.location);
    }
    verDoThrow(actor) =
    {
        if (not actor.isCarrying(self))
        {
            "%You're% not carrying "; self.thedesc; ". ";
        }
        else
            self.verifyRemove(actor);
    }
    doThrow(actor) =
    {
        "Thrown. ";
        self.moveInto(actor.location);
    }
    verDoShowTo(actor, io) =
    {
    }
    doShowTo(actor, io) =
    {
        if (io <> nil)
        {
            caps(); io.thedesc; " "; io.isntdesc; " impressed. ";
        }
    }
    verIoShowTo(actor) =
    {
        caps(); self.thedesc; " "; self.isntdesc; " impressed. ";
    }
    verDoClean(actor) =
    {
        caps(); self.thedesc; " looks a bit cleaner now. ";
    }
    verDoCleanWith(actor, io) = {}
    doCleanWith(actor, io) =
    {
        caps(); self.thedesc; " looks a bit cleaner now. ";
    }
    verDoMove(actor) =
    {
        "Moving "; self.thedesc; " doesn't reveal anything. ";
    }
    verDoMoveTo(actor, io) =
    {
        "Moving "; self.thedesc; " doesn't reveal anything. ";
    }
    verIoMoveTo(actor) =
    {
        "That doesn't get us anywhere. ";
    }
    verDoMoveWith(actor, io) =
    {
        "Moving "; self.thedesc; " doesn't reveal anything. ";
    }
    verIoMoveWith(actor) =
    {
        caps(); self.thedesc; " <<self.doesdesc>>n't seem to help. ";
    }
    verDoTypeOn(actor, io) =
    {
        "You should put what you want typed in double quotes, for
        example, TYPE \"HELLO\" ON KEYBOARD. ";
    }
    verDoTouch(actor) =
    {
        "Touching "; self.thedesc; " doesn't seem to have any effect. ";
    }
    verDoPoke(actor) =
    {
        "Poking "; self.thedesc; " doesn't seem to have any effect. ";
    }
    verDoBreak(actor) = {}
    doBreak(actor) =
    {
        "You'll have to tell me how to do that.";
    }
    genMoveDir = { "%You% can't seem to do that. "; }
    verDoMoveN(actor) = { self.genMoveDir; }
    verDoMoveS(actor) = { self.genMoveDir; }
    verDoMoveE(actor) = { self.genMoveDir; }
    verDoMoveW(actor) = { self.genMoveDir; }
    verDoMoveNE(actor) = { self.genMoveDir; }
    verDoMoveNW(actor) = { self.genMoveDir; }
    verDoMoveSE(actor) = { self.genMoveDir; }
    verDoMoveSW(actor) = { self.genMoveDir; }
    verDoSearch(actor) =
    {
        "%You% find%s% nothing of interest. ";
    }

    /* on dynamic construction, move into my contents list */
    construct =
    {
        self.moveInto(location);
    }

    /* on dynamic destruction, move out of contents list */
    destruct =
    {
        self.moveInto(nil);
    }

    /*
     *   Make it so that the player can give a command to an actor only
     *   if an actor is reachable in the normal manner.  This method
     *   returns true when 'self' can be given a command by the player. 
     */
    validActor = (self.isReachable(parserGetMe()))
;

/*
 *  item: thing
 *
 *  A basic item which can be picked up by the player.  It has no weight
 *  (0) and minimal bulk (1).  The weight property should be set
 *  to a non-zero value for heavy objects.  The bulk property
 *  should be set to a value greater than 1 for bulky objects, and to
 *  zero for objects that are very small and take essentially no effort
 *  to hold---or, more precisely, don't detract at all from the player's
 *  ability to hold other objects (for example, a piece of paper).
 */
class item: thing
    weight = 0
    bulk = 1
;
    
/*
 *  lightsource: item
 *
 *  A portable lamp, candle, match, or other source of light.  The
 *  light source can be turned on and off with the islit property.
 *  If islit is true, the object provides light, otherwise it's
 *  just an ordinary object.  Note that this object provides a doTurnon
 *  method to provide appropriate behavior for a switchable light source,
 *  such as a flashlight or a room's electric lights.  However, this object
 *  does not provide a verDoTurnon method, so by default it can't be
 *  switched on and off.  To create something like a flashlight that should
 *  be a lightsource that can be switched on and off, simply include both
 *  lightsource and switchItem in the superclass list, and be sure
 *  that lightsource precedes switchItem in the superclass list,
 *  because the doTurnon method provided by lightsource should
 *  override the one provided by switchItem.  The doTurnon method
 *  provided here turns on the light source (by setting its isActive
 *  property to true, and then describes the room if it was previously
 *  dark.
 */
class lightsource: item
    islamp = true
    islit = nil
    doTurnon(actor) =
    {
        local waslit := actor.location.islit;
        
        // turn on the light
        self.isActive := true;
        self.islit := true;
        "%You% switch%es% on <<self.thedesc>>";
        
        // if the room wasn't previously lit, and it is now, describe it
        if (actor.location.islit and not waslit)
        {
            ", lighting the area.\b";
            actor.location.enterRoom(actor);
        }
        else
            ".";
    }
;

/*
 *  hiddenItem: object
 *
 *  This is an object that is hidden with one of the hider classes. 
 *  A hiddenItem object doesn't have any special properties in its
 *  own right, but all objects hidden with one of the hider classes
 *  must be of class hiddenItem so that initSearch can find
 *  them.
 */
class hiddenItem: object
;

/*
 *  hider: item
 *
 *  This is a basic class of object that can hide other objects in various
 *  ways.  The underHider, behindHider, and searchHider classes
 *  are examples of hider subclasses.  The class defines
 *  the method searchObj(actor, list), which is given the list
 *  of hidden items contained in the object (for example, this would be the
 *  underCont property, in the case of an underHider), and "finds"
 *  the object or objects. Its action is dependent upon a couple of other
 *  properties of the hider object.  The serialSearch property,
 *  if true, indicates that items in the list are to be found one at
 *  a time; if nil (the default), the entire list is found on the
 *  first try.  The autoTake property, if true, indicates that
 *  the actor automatically takes the item or items found; if nil, the
 *  item or items are moved to the actor's location.  The searchObj method
 *  returns the list with the found object or objects removed; the
 *  caller should assign this returned value back to the appropriate
 *  property (for example, underHider will assign the return value
 *  to underCont).
 *  
 *  Note that because the hider is hiding something, this class
 *  overrides the normal verDoSearch method to display the
 *  message, "You'll have to be more specific about how you want
 *  to search that."  The reason is that the normal verDoSearch
 *  message ("You find nothing of interest") leads players to believe
 *  that the object was exhaustively searched, and we want to avoid
 *  misleading the player.  On the other hand, we don't want a general
 *  search to be exhaustive for most hider objects.  So, we just
 *  display a message letting the player know that the search was not
 *  enough, but we don't give away what they have to do instead.
 *  
 *  The objects hidden with one of the hider classes must be
 *  of class hiddenItem.
 */
class hider: item
    verDoSearch(actor) =
    {
        "%You%'ll have to be more specific about how %you% want%s%
        to search <<self.thatdesc>>. ";
    }
    searchObj(actor, list) =
    {
        local found, dest, i, tot;
        
        /* see how much we get this time */
        if (self.serialSearch)
        {
            found := [] + car(list);
            list := cdr(list);
        }
        else
        {
            found := list;
            list := nil;
        }
        
        /* set it(them) to the found item(s) */
        if (length(found) = 1)
            setit(found[1]);    // only one item - set 'it'
        else
            setit(found);       // multiple items - set 'them'
        
        /* figure destination */
        dest := actor;
        if (not self.autoTake)
            dest := dest.location;
        
        /* note what we found, and move it to destination */
        "%You% find%s% ";
        tot := length(found);
        i := 1;
        while (i <= tot)
        {
            found[i].adesc;
            if (i+1 < tot)
                ", ";
            else if (i = 1 and tot = 2)
                " and ";
            else if (i+1 = tot and tot > 2)
                ", and ";
            
            found[i].moveInto(dest);
            i := i + 1;
        }
        
        /* say what happened */
        if (self.autoTake)
            ", which %you% take%s%. ";
        else
            "! ";
        
        if (list<>nil and length(list)=0)
            list := nil;
        return list;
    }
    serialSearch = nil             /* find everything in one try by default */
    autoTake = true               /* actor takes item when found by default */
;

/*
 *  underHider: hider
 *
 *  This is an object that can have other objects placed under it.  The
 *  objects placed under it can only be found by looking under the object;
 *  see the description of hider for more information.  You should
 *  set the underLoc property of each hidden object to point to
 *  the underHider.
 *  
 *  Note that an underHider doesn't allow the player to put anything
 *  under the object during the game.  Instead, it's to make it easy for the
 *  game writer to set up hidden objects while implementing the game.  All you
 *  need to do to place an object under another object is declare the top
 *  object as an underHider, then declare the hidden object normally,
 *  except use underLoc rather than location to specify the
 *  location of the hidden object.  The behindHider and searchHider
 *  objects work similarly.
 *  
 *  The objects hidden with underHider must be of class hiddenItem.
 */
class underHider: hider
    underCont = []         /* list of items under me (set up by initSearch) */
    verDoLookunder(actor) = { }
    doLookunder(actor) =
    {
        if (self.underCont = [])
            "There's nothing under <<self.thedesc>>. ";
        else if (self.underCont = nil)
            "There's nothing else under <<self.thedesc>>. ";
        else
            self.underCont := self.searchObj(actor, self.underCont);
    }
;

/*
 *  behindHider: hider
 *
 *  This is just like an underHider, except that objects are hidden
 *  behind this object.  Objects to be behind this object should have their
 *  behindLoc property set to point to this object.
 *  
 *  The objects hidden with behindHider must be of class hiddenItem.
 */
class behindHider: hider
    behindCont = []
    verDoLookbehind(actor) = {}
    doLookbehind(actor) =
    {
        if (self.behindCont = [])
            "There's nothing behind <<self.thedesc>>. ";
        else if (self.behindCont = nil)
            "There's nothing else behind <<self.thedesc>>. ";
        else
            self.behindCont := self.searchObj(actor, self.behindCont);
    }
;
    
/*
 *  searchHider: hider
 *
 *  This is just like an underHider, except that objects are hidden
 *  within this object in such a way that the object must be looked in
 *  or searched.  Objects to be hidden in this object should have their
 *  searchLoc property set to point to this object.  Note that this
 *  is different from a normal container, in that the objects hidden within
 *  this object will not show up until the object is explicitly looked in
 *  or searched.
 *  
 *  The items hidden with searchHider must be of class hiddenItem.
 */
class searchHider: hider
    searchCont = []
    verDoSearch(actor) = {}
    doSearch(actor) =
    {
        if (self.searchCont = [])
            "There's nothing in <<self.thedesc>>. ";
        else if (self.searchCont = nil)
            "There's nothing else in <<self.thedesc>>. ";
        else
            self.searchCont := self.searchObj(actor, self.searchCont);
    }
    verDoLookin(actor) =
    {
        if (self.searchCont = [] || self.searchCont = nil)
            pass verDoLookin;
    }
    doLookin(actor) =
    {
        if (self.searchCont = [] || self.searchCont = nil)
            pass doLookin;
        else
            self.searchCont := self.searchObj(actor, self.searchCont);
    }
;
    

/*
 *  fixeditem: thing
 *
 *  An object that cannot be taken or otherwise moved from its location.
 *  Note that a fixeditem is sometimes part of a movable object;
 *  this can be done to make one object part of another, ensuring that
 *  they cannot be separated.  By default, the functions that list a room's
 *  contents do not automatically describe fixeditem objects (because
 *  the isListed property is set to nil).  Instead, the game author
 *  will generally describe the fixeditem objects separately as part of
 *  the room's ldesc.  
 */
class fixeditem: thing      // An immovable object
    isListed = nil          // not listed in room/inventory displays
    isfixed = true          // Item can't be taken
    weight = 0              // no actual weight
    bulk = 0
    verDoTake(actor) =
    {
        "%You% can't have "; self.thedesc; ". ";
    }
    verDoTakeOut(actor, io) =
    {
        self.verDoTake(actor);
    }
    verDoDrop(actor) =
    {
        "%You% can't drop "; self.thedesc; ". ";
    }
    verDoTakeOff(actor, io) =
    {
        self.verDoTake(actor);
    }
    verDoPutIn(actor, io) =
    {
        "%You% can't put "; self.thedesc; " anywhere. ";
    }
    verDoPutOn(actor, io) =
    {
        "%You% can't put "; self.thedesc; " anywhere. ";
    }
    verDoMove(actor) =
    {
        "%You% can't move "; self.thedesc; ". ";
    }
    verDoThrowAt(actor, iobj) =
    {
        "%You% can't throw <<self.thedesc>>.";
    }
;

/*
 *  readable: item
 *
 *  An item that can be read.  The readdesc property is displayed
 *  when the item is read.  By default, the readdesc is the same
 *  as the ldesc, but the readdesc can be overridden to give
 *  a different message.
 */
class readable: item
    verDoRead(actor) =
    {
    }
    doRead(actor) =
    {
        self.readdesc;
    }
    readdesc =
    {
        self.ldesc;
    }
;

/*
 *  fooditem: item
 *
 *  An object that can be eaten.  When eaten, the object is removed from
 *  the game, and global.lastMealTime is decremented by the
 *  foodvalue property.  By default, the foodvalue property
 *  is global.eatTime, which is the time between meals.  So, the
 *  default fooditem will last for one "nourishment interval."
 */
class fooditem: item
    verDoEat(actor) =
    {
        self.verifyRemove(actor);
    }
    doEat(actor) =
    {
        "That was delicious! ";
        global.lastMealTime := global.lastMealTime - self.foodvalue;
        self.moveInto(nil);
    }
    foodvalue = { return global.eatTime; }
;

/*
 *  dialItem: fixeditem
 *
 *  This class is used for making "dials," which are controls in
 *  your game that can be turned to a range of numbers.  You can
 *  define the property maxsetting as a number specifying the
 *  highest number to which the dial can be turned; the lowest number
 *  on the dial is minsetting.  The setting property is the dial's
 *  current setting, and can be changed by the player by typing the
 *  command "turn dial to number."  By default, the ldesc
 *  method displays the current setting.
 */
class dialItem: fixeditem
    minsetting = 1  // it has settings from this value...
    maxsetting = 10 // ...to this value
    setting = 1     // the current setting
    ldesc =
    {
        caps(); self.thedesc; " can be turned to settings
        numbered from << self.minsetting >> to << self.maxsetting >>.
        \^<<self.itisdesc>> currently set to << self.setting >>. ";
    }
    verDoTurn(actor) = {}
    doTurn(actor) =
    {
        askio(toPrep);
    }
    verDoTurnTo(actor, io) = {}
    doTurnTo(actor, io) =
    {
        if (io = numObj)
        {
            if (numObj.value < self.minsetting
                or numObj.value > self.maxsetting)
            {
                "There's no such setting! ";
            }
            else if (numObj.value <> self.setting)
            {
                self.setting := numObj.value;
                "Okay, <<self.itisdesc>> now turned to ";
                say(self.setting); ". ";
            }
            else
            {
                "\^<<self.itisdesc>> already set to ";
                 say(self.setting); "! ";
            }
        }
        else
        {
            "I don't know how to turn "; self.thedesc;
            " to that. ";
        }
    }
;

/*
 *  switchItem: fixeditem
 *
 *  This is a class for things that can be turned on and off by the
 *  player.  The only special property is isActive, which is nil
 *  if the switch is turned off and true when turned on.  The object
 *  accepts the commands "turn it on" and "turn it off," as well as
 *  synonymous constructions, and updates isActive accordingly.
 */
class switchItem: fixeditem
    verDoSwitch(actor) = {}
    doSwitch(actor) =
    {
        self.isActive := not self.isActive;
        "Okay, "; self.thedesc; " <<self.isdesc>> now switched ";
        if (self.isActive)
            "on";
        else
            "off";
        ". ";
    }
    verDoTurnon(actor) =
    {
        /*
         *   You can't turn on something in the dark unless you're
         *   carrying it.  You also can't turn something on if it's
         *   already on.  
         */
        if (not actor.location.islit and not actor.isCarrying(self))
            "It's pitch black.";
        else if (self.isActive)
            "\^<<self.itisdesc>> already turned on! ";
    }
    doTurnon(actor) =
    {
        self.isActive := true;
        "Okay, <<self.itisdesc>> now turned on. ";
    }
    verDoTurnoff(actor) =
    {
        if (not self.isActive)
            "\^<<self.itisdesc>> already turned off! ";
    }
    doTurnoff(actor) =
    {
        self.isActive := nil;
        "Okay, <<self.itisdesc>> now turned off. ";
    }
;

/*
 *  room: thing
 *
 *  A location in the game.  By default, the islit property is
 *  true, which means that the room is lit (no light source is
 *  needed while in the room).  You should create a darkroom
 *  object rather than a room with islit set to nil if you
 *  want a dark room, because other methods are affected as well.
 *  The isseen property records whether the player has entered
 *  the room before; initially it's nil, and is set to true
 *  the first time the player enters.  The roomAction(actor,
 *  verb, directObject, preposition, indirectObject) method is
 *  activated for each player command; by default, all it does is
 *  call the room's location's roomAction method if the room
 *  is inside another room.  The lookAround(verbosity)
 *  method displays the room's description for a given verbosity
 *  level; true means a full description, nil means only
 *  the short description (just the room name plus a list of the
 *  objects present).  roomDrop(object) is called when
 *  an object is dropped within the room; normally, it just moves
 *  the object to the room and displays "Dropped."  The firstseen
 *  method is called when isseen is about to be set true
 *  for the first time (i.e., when the player first sees the room);
 *  by default, this routine does nothing, but it's a convenient
 *  place to put any special code you want to execute when a room
 *  is first entered.  The firstseen method is called after
 *  the room's description is displayed.
 */
class room: thing
    /*
     *   'reachable' is the list of explicitly reachable objects, over and
     *   above the objects that are here.  This is mostly used in nested
     *   rooms to make objects in the containing room accessible.  Most
     *   normal rooms will leave this as an empty list.
     */
    reachable = []

    /*
     *   The default message when the actor can't reach something in the
     *   room.  If we got here, it means that the object that the actor
     *   was actually trying to reach is within the room, or within
     *   something inside the room whose contents are accessible. 
     */
    cantReach(actor) =
    {
        if (actor.location = self || actor.location = nil)
        {
            /*
             *   The actor is in the room, and not inside any other object
             *   within the room.  The object must simply not be
             *   accessible for some reason. 
             */
            "I don't see that here.";
        }
        else
        {
            /*
             *   The actor is inside something from which the room is
             *   visible (such as something within the room), but from
             *   which the room's contents are not reachable.  Tell the
             *   actor's location to generate an appropriate message. 
             */
            actor.location.cantReachRoom(self);
        }
    }

    /*
     *   This routine is called when the actor is (directly) within this
     *   object, and is trying to reach something in the given room, but
     *   cannot.  This should generate an appropriate message explaining
     *   why objects in the given room are visible but not reachable.  By
     *   default, we'll just say that the object is not reachable; some
     *   rooms may wish to explain more fully why this is the case. 
     */
    cantReachRoom(otherRoom) =
    {
        "%You% can't reach that from here. ";
    }
        
    /*
     *   roomCheck is true if the verb is valid in the room.  This
     *   is a first pass; generally, its only function is to disallow
     *   certain commands in a dark room.
     */
    roomCheck(v) = { return true; }
    islit = true            // rooms are lit unless otherwise specified
    isseen = nil            // room has not been seen yet
    enterRoom(actor) =      // sent to room as actor is entering it
    {
        self.lookAround((not self.isseen) or global.verbose);
        if (self.islit)
        {
            if (not self.isseen)
                self.firstseen;
            self.isseen := true;
        }
    }

    /*
     *   Notify this object that the current player character ("me")
     *   object is being explicitly moved into this room via the player
     *   character object's moveInto() method.  This method is only called
     *   when the player character is explicitly moved into this location.
     *   This method doesn't do anything by default; it is provided for
     *   cases where the room must do something special when the player
     *   character is moved into it.
     *   
     *   Note that this is called BEFORE the player character's new
     *   location is established, so that we still have access to the
     *   player character's old location during this method.  (Note that
     *   we do know the new location here - it's always 'self'.)  
     */
    meIsMovingInto(meActor) =
    {
        /* default does nothing */
    }
    
    roomAction(a, v, d, p, i) =
    {
        if (self.location)
            self.location.roomAction(a, v, d, p, i);
    }

    /*
     *   Whenever a normal object (i.e., one that does not override the
     *   default doDrop provided by 'thing') is dropped, the actor's
     *   location is sent roomDrop(object being dropped).  By default, 
     *   we move the object into this room.
     */
    roomDrop(obj) =
    {
        "Dropped. ";
        obj.moveInto(self);
    }

    /*
     *   Whenever an actor leaves this room, we run through the leaveList.
     *   This is a list of objects that have registered themselves with us
     *   via addLeaveList().  For each object in the leaveList, we send
     *   a "leaving" message, with the actor as the parameter.  It should
     *   return true if it wants to be removed from the leaveList, nil
     *   if it wants to stay.
     */
    leaveList = []
    addLeaveList(obj) =
    {
        self.leaveList := self.leaveList + obj;
    }
    leaveRoom(actor) =
    {
        local tmplist, thisobj, i, tot;
        
        tmplist := self.leaveList;
        tot := length(tmplist);
        i := 1;
        while (i <= tot)
        {
            thisobj := tmplist[i];
            if (thisobj.leaving(actor))
                self.leaveList := self.leaveList - thisobj;
            i := i + 1;
        }
    }

    /*
     *   dispParagraph - display the paragraph separator.  By default we
     *   display a \n\t sequence; games can change this (via 'modify') to
     *   customize the display format 
     */
    dispParagraph = "\n\t"

    /*
     *   dispBeginSdesc and dispEndSdesc - display begin and end of
     *   location short description sequence.  We'll call these just
     *   before and after showing a room's description.  By default these
     *   do nothing; games that want to customize the display format can
     *   change these (via 'modify').  
     */
    dispBeginSdesc = ""
    dispEndSdesc = ""

    /*
     *   dispBeginLdesc and dispEndLdesc - display begin and end of
     *   location long description sequence.  We'll call these just before
     *   and after showing a room's long description.  By default, we
     *   start a new paragraph before the ldesc; games that want to
     *   customize the display format can change these (via 'modify').  
     */
    dispBeginLdesc = { self.dispParagraph; }
    dispEndLdesc = ""

    /*
     *   lookAround describes the room.  If verbosity is true, the full
     *   description is given, otherwise an abbreviated description (without
     *   the room's ldesc) is displayed.
     */
    nrmLkAround(verbosity) =        // lookAround without location status
    {
        local l, cur, i, tot;

        if (verbosity)
        {
            self.dispBeginLdesc;
            self.ldesc;
            self.dispEndLdesc;

            l := self.contents;
            tot := length(l);
            i := 1;
            while (i <= tot)
            {
                cur := l[i];
                if (cur.isfixed)
                    cur.heredesc;
                i := i + 1;
            }
        }

        self.dispParagraph;
        if (itemcnt(self.contents))
        {
            "%You% see%s% "; listcont(self); " here. ";
        }
        listcontcont(self); "\n";

        l := self.contents;
        tot := length(l);
        i := 1;
        while (i <= tot)
        {
            cur := l[i];
            if (cur.isactor)
            {
                if (cur <> parserGetMe())
                {
                    self.dispParagraph;
                    cur.actorDesc;
                }
            }
            i := i + 1;
        }
        "\n";
    }
#ifdef USE_HTML_STATUS

    /* 
     *   Generate HTML formatting that provides the traditional status
     *   line appearance, using the traditional status line computation
     *   mechanism.
     */
    statusLine =
    {
        /*
         *   Check the system to see if this is an HTML-enabled run-time.
         *   If so, generate an HTML-style banner; otherwise, generate an
         *   old-style banner. 
         */
        if (systemInfo(__SYSINFO_SYSINFO) = true
            and systemInfo(__SYSINFO_HTML) = 1)
        {
            "<banner id=StatusLine height=previous border>
            <body bgcolor=statusbg text=statustext><b>";

            self.statusRoot;
            
            "</b><tab align=right><i>";

            say(scoreFormat(global.score, global.turnsofar));
            " ";
        
            "</i></banner>";
        }
        else
        {
            self.statusRoot;
            self.dispParagraph;
        }
    }
    
#else /* USE_HTML_STATUS */

    /* use the standard HTML status line system */
    statusLine =
    {
        self.statusRoot;
        self.dispParagraph;
    }

#endif /* USE_HTML_STATUS */

    statusRoot =
    {
        self.sdesc;
    }
    lookAround(verbosity) =
    {
        self.dispBeginSdesc;
        self.statusRoot;
        self.dispEndSdesc;
        
        self.nrmLkAround(verbosity);
    }
    
    /*
     *   Direction handlers for the player character.  The directions are
     *   all set up to the default, which is no travel allowed.  To make
     *   travel possible in a direction, just assign a room to the
     *   direction property.  
     */
    north = { return self.noexit; }
    south = { return self.noexit; }
    east  = { return self.noexit; }
    west  = { return self.noexit; }
    up    = { return self.noexit; }
    down  = { return self.noexit; }
    ne    = { return self.noexit; }
    nw    = { return self.noexit; }
    se    = { return self.noexit; }
    sw    = { return self.noexit; }
    in    = { return self.noexit; }
    out   = { return self.noexit; }

    /*
     *   Direction handlers for non-player characters.  By default, we
     *   handle each direction for an NPC the same way we handle the
     *   corresponding direction for the player character.  A room can
     *   override any of these it wants to differentiate between PC travel
     *   and NPC travel. 
     */
    actorNorth = { return self.north; }
    actorSouth = { return self.south; }
    actorEast  = { return self.east; }
    actorWest  = { return self.west; }
    actorUp    = { return self.up; }
    actorDown  = { return self.down; }
    actorNE    = { return self.ne; }
    actorNW    = { return self.nw; }
    actorSE    = { return self.se; }
    actorSW    = { return self.sw; }
    actorIn    = { return self.in; }
    actorOut   = { return self.out; }

    /*
     *   noexit displays a message when the player attempts to travel
     *   in a direction in which travel is not possible.
     */
    noexit = { "%You% can't go that way. "; return nil; }
;

/*
 *  darkroom: room
 *
 *  A dark room.  The player must have some object that can act as a
 *  light source in order to move about and perform most operations
 *  while in this room.  Note that the room's lights can be turned
 *  on by setting the room's lightsOn property to true;
 *  do this instead of setting islit, because islit is
 *  a method which checks for the presence of a light source.
 */
class darkroom: room        // An enterable area which might be dark
    islit =                 // true ONLY if something is lighting the room
    {
        local rem, cur, tot, i;

        if (self.lightsOn)
            return true;
        
        rem := global.lamplist;
        tot := length(rem);
        i := 1;
        while (i <= tot)
        {
            cur := rem[i];
            if (cur.isIn(self) and cur.islit)
                return true;
            i := i + 1;
        }
        return nil;
    }
    roomAction(actor, v, dobj, prep, io) =
    {
        if (not self.islit and not v.isDarkVerb)
        {
            "%You% can't see a thing. ";
            exit;
        }
        else
            pass roomAction;
    }
    statusRoot =
    {
        if (self.islit)
            pass statusRoot;
        else
            "In the dark.";
    }
    lookAround(verbosity) =
    {
        if (self.islit)
            pass lookAround;
        else
            "It's pitch black. ";
    }
    noexit =
    {
        if (self.islit)
            pass noexit;
        else
        {
            darkTravel();
            return nil;
        }
    }
    roomCheck(v) =
    {
        if (self.islit or v.isDarkVerb)
            return true;
        else
        {
            "It's pitch black.\n";
            return nil;
        }
    }
;

/*
 *  theFloor is a special item that appears in every room (hence
 *  the non-standard location property).  This object is included
 *  mostly for completeness, so that the player can refer to the
 *  floor; otherwise, it doesn't do much.  Dropping an item on the
 *  floor, for example, moves it to the current room.
 */
theFloor: beditem, floatingItem
    noun = 'floor' 'ground'
    sdesc = "ground"
    ldesc = "It lies beneath %youm%. "
    adesc = "the ground"
    statusPrep = "on"
    outOfPrep = "off of"
    location =
    {
        if (parserGetMe().location = self)
            return self.sitloc;
        else
            return parserGetMe().location;
    }
    locationOK = true        // suppress warning about location being a method
    doSiton(actor) =
    {
        "Okay, %you're% now sitting on "; self.thedesc; ". ";
        self.sitloc := actor.location;
        actor.moveInto(self);
    }
    meIsMovingInto(meActor) =
    {
        /*
         *   When the player character object is explicitly moved into
         *   theFloor, we must set our sitloc property to the player
         *   character's old location.  Since we're floating, we don't
         *   have a location ourselves, so we use sitloc to keep track of
         *   the effective container that the player should see when
         *   moving back out of the floor. 
         */
        self.sitloc := meActor.location;
    }

    doLieon(actor) =
    {
        self.doSiton(actor);
    }
    ioPutOn(actor, dobj) =
    {
        dobj.doDrop(actor);
    }
    ioPutIn(actor, dobj) =
    {
        dobj.doDrop(actor);
    }
    ioThrowAt(actor, dobj) =
    {
        "Thrown. ";
        dobj.moveInto(actor.location);
    }

    /* 
     *   when the actor is on the floor, throwing an object makes the
     *   object land in the current floating location 
     */
    throwHitDest = { return self.location; }
;

/*
 *  Actor: fixeditem, movableActor
 *
 *  A character in the game.  The maxweight property specifies
 *  the maximum weight that the character can carry, and the maxbulk
 *  property specifies the maximum bulk the character can carry.  The
 *  actorAction(verb, directObject, preposition, indirectObject)
 *  method specifies what happens when the actor is given a command by
 *  the player; by default, the actor ignores the command and displays
 *  a message to this effect.  The isCarrying(object)
 *  method returns true if the object is being carried by
 *  the actor.  The actorDesc method displays a message when the
 *  actor is in the current room; this message is displayed along with
 *  a room's description when the room is entered or examined.  The
 *  verGrab(object) method is called when someone tries to
 *  take an object the actor is carrying; by default, an actor won't
 *  let other characters take its possessions.
 *  
 *  If you want the player to be able to follow the actor when it
 *  leaves the room, you should define a follower object to shadow
 *  the character, and set the actor's myfollower property to
 *  the follower object.  The follower is then automatically
 *  moved around just behind the actor by the actor's moveInto
 *  method.
 *  
 *  The isHim property should return true if the actor can
 *  be referred to by the player as "him," and likewise isHer
 *  should be set to true if the actor can be referred to as "her."
 *  Note that both or neither can be set; if neither is set, the actor
 *  can only be referred to as "it," and if both are set, any of "him,"
 *  "her," or "it" will be accepted.
 */
class Actor: fixeditem, movableActor
;

/*
 *  movableActor: qcontainer
 *
 *  Just like an Actor object, except that the player can
 *  manipulate the actor like an ordinary item.  Useful for certain
 *  types of actors, such as small animals.
 */
class movableActor: qcontainer // A character in the game
    isListed = nil          // described separately from room's contents
    weight = 10             // actors are pretty heavy
    bulk = 10               // and pretty bulky
    maxweight = 50          // Weight that can be carried at once
    maxbulk = 20            // Number of objects that can be carried at once
    isactor = true          // flag that this is an actor
    reachable = []
    roomCheck(v) =
    {
        /*
         *   if the actor has a location, return its roomCheck, otherwise
         *   simply allow the command 
         */
        if (self.location)
            return self.location.roomCheck(v);
        else
            return true;
    }
    actorAction(v, d, p, i) =
    {
        "\^<<self.thedesc>> <<self.doesdesc>>n't appear interested. ";
        exit;
    }
    isCarrying(obj) = { return obj.isIn(self); }
    actorDesc =
    {
        "\^<<self.adesc>> is here. ";
    }
    verGrab(item) =
    {
        "\^<<self.thedesc>> <<self.isdesc>> carrying <<item.thedesc>> and
        won't let %youm% have <<item.itobjdesc>>. ";
    }
    verDoFollow(actor) =
    {
        "But <<self.thedesc>> is right here! ";
    }
    moveInto(obj) =
    {
        if (self.myfollower)
            self.myfollower.moveInto(self.location);
        pass moveInto;
    }
    // these properties are for the format strings
    fmtYou = { self.isHim ? "he" : self.isHer ? "she" : "it"; }
    fmtYour = { self.isHim ? "his" : self.isHer ? "her" : "its"; }
    fmtYoure = { self.isHim ? "he's" : self.isHer ? "she's" : "it's"; }
    fmtYoum = { self.isHim ? "him" : self.isHer ? "her" : "it"; }
    fmtYouve = { self.isHim ? "he's" : self.isHer ? "she's" : "it's"; }
    fmtS = "s"
    fmtEs = "es"
    fmtHave = "has"
    fmtDo = "does"
    fmtAre = "is"
    fmtMe = { self.thedesc; }
    askWord(word, lst) = { return nil; }
    verDoAskAbout(actor, iobj) = {}
    doAskAbout(actor, iobj) =
    {
        local lst, i, tot;
        
        lst := objwords(2);       // get actual words asked about
        tot := length(lst);
        if ((tot = 1 and (find(['it' 'them' 'him' 'her'], lst[1]) <> nil))
            or tot = 0)
        {
            "\"Could you be more specific?\"";
            return;
        }
        
        // try to find a response for each word
        for (i := 1 ; i <= tot ; ++i)
        {
            if (self.askWord(lst[i], lst))
                return;
        }
        
        // didn't find anything to talk about
        self.disavow;
    }
    disavow = "\"I don't know much about that.\""
    verIoPutIn(actor) =
    {
        "If you want something given to << thedesc >>, just say so.";
    }
    verIoGiveTo(actor) =
    {
        if (actor = self)
            "That wouldn't accomplish anything!";
    }
    ioGiveTo(actor, dobj) =
    {
        "\^<<self.thedesc>> rejects the offer.";
    }

    // move to a new location, notifying player of coming and going
    travelTo(room) =
    {
        local old_loc_visible;
        local new_loc_visible;
        
        /* if it's an obstacle, travel to the obstacle instead */
        while (room != nil && room.isobstacle)
            room := room.destination;
        
        /* do nothing if going nowhere */
        if (room = nil)
            return;

        /* note whether the actor was previously visible */
        old_loc_visible := (self.location != nil
                            && parserGetMe().isVisible(self.location));

        /* note whether the actor will be visible in the new location */
        new_loc_visible := parserGetMe().isVisible(room);

        /* 
         *   if I'm leaving the player's location, and I was previously
         *   visible, and I'm not going to be visible after the move, and
         *   the player's location isn't dark, show the "leaving" message 
         */
        if (parserGetMe().location != nil
            && parserGetMe().location.islit
            && old_loc_visible
            && !new_loc_visible)
            self.sayLeaving;
        
        /* move to my new location */
        self.moveInto(room);

        /* 
         *   if I'm visible to the player at the new location, and I
         *   wasn't previously visible, and it's not dark, show the
         *   "arriving" message 
         */
        if (parserGetMe().location != nil
            && parserGetMe().location.islit
            && new_loc_visible
            && !old_loc_visible)
            self.sayArriving;
    }

    // sayLeaving and sayArriving announce the actor's departure and arrival
    // in the same room as the player.
    sayLeaving =
    {
        self.location.dispParagraph;
        "\^<<self.thedesc>> leaves the area.";
    }
    sayArriving =
    {
        self.location.dispParagraph;
        "\^<<self.thedesc>> enters the area.";
    }

    // this should be used as an actor when ambiguous
    preferredActor = true

    /*
     *   Mappings to room properties that we use when this actor travels
     *   via a directional command (NORTH, SOUTH, etc).  When we process an
     *   actor travel command, we'll look in these properties of the actor
     *   first.  Each of these will in turn give us another property, which
     *   we'll invoke on the actor's current location.
     *   
     *   Note that by default, the basic "Me" actor uses the basic
     *   directional properties (&north, &south, and so on), whereas
     *   non-player characters use the actor-specific directional
     *   properties (&actorNorth, &actorSouth, etc).  This means that when
     *   the player character travels, we'll find out the destination of
     *   the travel by calling
     *   
     *   Me.location.north
     *   
     *.  ..and when a non-player character travels, we'll instead call
     *   
     *   Me.location.actorNorth
     *   
     *   By default, room simply maps actorNorth to north, which means that
     *   the player character and the NPC's all end up acting the same way,
     *   so this distinction ends up making no difference.  However, the
     *   distinction makes it possible to make traveling act one way for
     *   the player character and another way for NPC's simply by
     *   overriding actorNorth (etc) in a room definition.  
     */
    northProp = &actorNorth
    southProp = &actorSouth
    eastProp = &actorEast
    westProp = &actorWest
    neProp = &actorNE
    nwProp = &actorNW
    seProp = &actorSE
    swProp = &actorSW
    inProp = &actorIn
    outProp = &actorOut
    upProp = &actorUp
    downProp = &actorDown
;

/*
 *  follower: Actor
 *
 *  This is a special object that can "shadow" the movements of a
 *  character as it moves from room to room.  The purpose of a follower
 *  is to allow the player to follow an actor as it leaves a room by
 *  typing a "follow" command.  Each actor that is to be followed must
 *  have its own follower object.  The follower object should
 *  define all of the same vocabulary words (nouns and adjectives) as the
 *  actual actor to which it refers.  The follower must also
 *  define the myactor property to be the Actor object that
 *  the follower follows.  The follower will always stay
 *  one room behind the character it follows; no commands are effective
 *  with a follower except for "follow."
 */
class follower: Actor
    sdesc = { self.myactor.sdesc; }
    isfollower = true
    ldesc = { caps(); self.myactor.thedesc; " <<self.myactor.isdesc>> no
             longer here. "; }
    actorAction(v, d, p, i) = { self.ldesc; exit; }
    actorDesc = {}
    myactor = nil   // set to the Actor to be followed
    verDoFollow(actor) = {}
    doFollow(actor) =
    {
        actor.travelTo(self.myactor.location);
    }
    dobjGen(a, v, i, p) =
    {
        if (v <> followVerb)
        {
            "\^<< self.myactor.thedesc >> <<self.myactor.isdesc>> no
            longer here.";
            exit;
        }
    }
    iobjGen(a, v, d, p) =
    {
        "\^<< self.myactor.thedesc >> <<self.myactor.isdesc>> no
        longer here.";
        exit;
    }
;

/*
 *  basicMe: Actor
 *
 *  A default implementation of the Me object, which is the
 *  player character.  adv.t defines basicMe instead of
 *  Me to allow your game to override parts of the default
 *  implementation while still using the rest, and without changing
 *  adv.t itself.  To use basicMe unchanged as your player
 *  character, include this in your game:  "Me: basicMe;".
 *  
 *  The basicMe object defines all of the methods and properties
 *  required for an actor, with appropriate values for the player
 *  character.  The nouns "me" and "myself" are defined ("I"
 *  is not defined, because it conflicts with the "inventory"
 *  command's minimal abbreviation of "i" in certain circumstances,
 *  and is generally not compatible with the syntax of most player
 *  commands anyway).  The sdesc is "you"; the thedesc
 *  and adesc are "yourself," which is appropriate for most
 *  contexts.  The maxbulk and maxweight properties are
 *  set to 10 each; a more sophisticated Me might include the
 *  player's state of health in determining the maxweight and
 *  maxbulk properties.
 */
class basicMe: Actor, floatingItem
    roomCheck(v) = { return self.location.roomCheck(v); }
    noun = 'me' 'myself'
    sdesc = "you"
    thedesc = "yourself"
    adesc = "yourself"
    ldesc = "You look about the same as always. "
    maxweight = 10
    maxbulk = 10
    verDoFollow(actor) =
    {
        if (actor = self)
            "You can't follow yourself! ";
    }
    actorAction(verb, dobj, prep, iobj) = 
    {
    }
    travelTo(room) =
    {
        /* 
         *   if I'm not the current player character, inherit the default
         *   handling 
         */
        if (parserGetMe() != self)
        {
            /* 
             *   I'm not the current player character, so I'm just an
             *   ordinary actor - use the inherited version that ordinary
             *   actors use 
             */
            inherited.travelTo(room);
            return;
        }

        /* 
         *   if the room is not nil, travel to the new location; ignore
         *   attempts to travel to "nil", because direction properties and
         *   obstacles return nil after displaying a message explaining
         *   why the travel is impossible, hence we need do nothing more
         *   in these cases 
         */
        if (room != nil)
        {
            if (room.isobstacle)
            {
                /* it's an obstacle - travel to the obstacle's destination */
                self.travelTo(room.destination);
            }
            else if (!(self.location.islit || room.islit))
            {
                /* neither location is lit - travel in the dark */
                darkTravel();
            }
            else
            {
                /* notify the old location that we're leaving */
                if (self.location != nil)
                    self.location.leaveRoom(self);

                /* move to the new location */
                self.location := room;

                /* notify the new location that we're entering */
                room.enterRoom(self);
            }
        }
    }
    moveInto(room) =
    {
        /*
         *   If I"m not the current player character, inherit the default
         *   handling; otherwise, we must use special handling, because
         *   the current player character never appears in its location's
         *   contents list. 
         */
        if (parserGetMe() = self)
        {
            /*
             *   Notify the new container that the player character is
             *   being explicitly moved into it. 
             */
            if (room != nil)
                room.meIsMovingInto(self);

            /* 
             *   we're the player character - move directly to the new
             *   room; do not update the contents lists for my old or new
             *   containers, because we do not appear in the old list and
             *   do not want to appear in the new list 
             */
            self.location := room;
        }
        else
        {
            /* 
             *   we're an ordinary actor at the moment - inherit the
             *   default handling 
             */
            inherited.moveInto(room);
        }
    }
    ioGiveTo(actor, dobj) =
    {
        "\^<<self.fmtYou>> accept<<self.fmtS>> <<dobj.thedesc
        >> from %youm%.";
        dobj.moveInto(parserGetMe());
    }

    // these properties are for the format strings
    fmtYou = "you"
    fmtYour = "your"
    fmtYoure = "you're"
    fmtYoum = "you"
    fmtYouve = "you've"
    fmtS = ""
    fmtEs = ""
    fmtHave = "have"
    fmtDo = "do"
    fmtAre = "are"
    fmtMe = "me"

    /* for travel, use the basic direction proeprties */
    northProp = &north
    southProp = &south
    eastProp = &east
    westProp = &west
    neProp = &ne
    nwProp = &nw
    seProp = &se
    swProp = &sw
    inProp = &in
    outProp = &out
    upProp = &up
    downProp = &down
;

/*
 *  decoration: fixeditem
 *
 *  An item that doesn't have any function in the game, apart from
 *  having been mentioned in the room description.  These items
 *  are immovable and can't be manipulated in any way, but can be
 *  referred to and inspected.  Liberal use of decoration items
 *  can improve a game's playability by helping the parser recognize
 *  all the words the game uses in its descriptions of rooms.
 */
class decoration: fixeditem
    ldesc = "\^<<self.thatdesc>> <<self.isntdesc>> important."
    dobjGen(a, v, i, p) =
    {
        if (v <> inspectVerb)
        {
            "\^<<self.thedesc>> <<self.isntdesc>> important.";
            exit;
        }
    }
    iobjGen(a, v, d, p) =
    {
        "\^<<self.thedesc>> <<self.isntdesc>> important.";
        exit;
    }
;

/*
 *  distantItem: fixeditem
 *
 *  This is an item that is too far away to manipulate, but can be seen.
 *  The class uses dobjGen and iobjGen to prevent any verbs from being
 *  used on the object apart from inspectVerb; using any other verb results
 *  in the message "It's too far away."  Instances of this class should
 *  provide the normal item properties:  sdesc, ldesc, location,
 *  and vocabulary.
 */
class distantItem: fixeditem
    dobjGen(a, v, i, p) =
    {
        if (v <> askVerb and v <> tellVerb and v <> inspectVerb)
        {
            "\^<<self.itisdesc>> too far away.";
            exit;
        }
    }
    iobjGen(a, v, d, p) = { self.dobjGen(a, v, d, p); }
;

/*
 *  buttonitem: fixeditem
 *
 *  A button (the type you push).  The individual button's action method
 *  doPush(actor), which must be specified in
 *  the button, carries out the function of the button.  Note that
 *  all buttons have the noun "button" defined.
 */
class buttonitem: fixeditem
    noun = 'button'
    plural = 'buttons'
    verDoPush(actor) = {}
;

/*
 *  clothingItem: item
 *
 *  Something that can be worn.  By default, the only thing that
 *  happens when the item is worn is that its isworn property
 *  is set to true.  If you want more to happen, override the
 *  doWear(actor) property.  Note that, when a clothingItem
 *  is being worn, certain operations will cause it to be removed (for
 *  example, dropping it causes it to be removed).  If you want
 *  something else to happen, override the checkDrop method;
 *  if you want to disallow such actions while the object is worn,
 *  use an exit statement in the checkDrop method.
 */
class clothingItem: item
    checkDrop =
    {
        if (self.isworn)
        {
            "(Taking off "; self.thedesc; " first)\n";
            self.isworn := nil;
        }
    }
    doDrop(actor) =
    {
        self.checkDrop;
        pass doDrop;
    }
    doPutIn(actor, io) =
    {
        self.checkDrop;
        pass doPutIn;
    }
    doPutOn(actor, io) =
    {
        self.checkDrop;
        pass doPutOn;
    }
    doGiveTo(actor, io) =
    {
        self.checkDrop;
        pass doGiveTo;
    }
    doThrowAt(actor, io) =
    {
        self.checkDrop;
        pass doThrowAt;
    }
    doThrowTo(actor, io) =
    {
        self.checkDrop;
        pass doThrowTo;
    }
    doThrow(actor) =
    {
        self.checkDrop;
        pass doThrow;
    }
    moveInto(obj) =
    {
        /*
         *   Catch any other movements with moveInto; this won't stop the
         *   movement from happening, but it will prevent any anamolous
         *   consequences caused by the object moving but still being worn.
         */
        self.isworn := nil;
        pass moveInto;
    }
    verDoWear(actor) =
    {
        if (self.isworn && self.isIn(actor))
        {
            "%You're% already wearing "; self.thedesc; "! ";
        }
    }
    doWear(actor) =
    {
        /* 
         *   if the actor is not directly carrying it (in other words,
         *   it's either not in the player's inventory at all, or it's
         *   within a container within the actor's inventory), take it --
         *   the actor must be directly holding the object for this to
         *   succeed 
         */
        if (self.location != actor)
        {
            /* try taking it */
            "(First taking <<self.thedesc>>)\n";
            if (execCommand(actor, takeVerb, self) != 0)
                exit;

            /* 
             *   make certain it ended up where we want it - the command
             *   might have failed without actually indicating failure 
             */
            if (self.location != actor)
                exit;
        }

        /* wear it */
        "Okay, %you're% now wearing "; self.thedesc; ". ";
        self.isworn := true;
    }
    verDoUnwear(actor) =
    {
        if (not self.isworn or not self.isIn(actor))
        {
            "%You're% not wearing "; self.thedesc; ". ";
        }
    }
    doUnwear(actor) =
    {
        /* ensure that maximum bulk is not exceeded */
        if (addbulk(actor.contents) + self.bulk > actor.maxbulk)
        {
            "%You% could drop <<self.thedesc>>, but %your% hands are
            too full to carry <<self.itobjdesc>>. ";
        }
        else
        {
            /* doff the item */
            "Okay, %you're% no longer wearing "; self.thedesc; ". ";
            self.isworn := nil;
        }
    }
    verDoTake(actor) =
    {
        if (self.isworn)
            self.verDoUnwear(actor);
        else
            pass verDoTake;
    }
    doTake(actor) =
    {
        if (self.isworn)
            self.doUnwear(actor);
        else
            pass doTake;
    }
    doSynonym('Unwear') = 'Unboard'
;

/*
 *  obstacle: object
 *
 *  An obstacle is used in place of a room for a direction
 *  property.  The destination property specifies the room that
 *  is reached if the obstacle is successfully negotiated; when the
 *  obstacle is not successfully negotiated, destination should
 *  display an appropriate message and return nil.
 */
class obstacle: object
    isobstacle = true
;

/*
 *  doorway: fixeditem, obstacle
 *
 *  A doorway is an obstacle that impedes progress when it is closed.
 *  When the door is open (isopen is true), the user ends up in
 *  the room specified in the doordest property upon going through
 *  the door.  Since a doorway is an obstacle, use the door object for
 *  a direction property of the room containing the door.
 *  
 *  If noAutoOpen is not set to true, the door will automatically
 *  be opened when the player tries to walk through the door, unless the
 *  door is locked (islocked = true).  If the door is locked,
 *  it can be unlocked simply by typing "unlock door", unless the
 *  mykey property is set, in which case the object specified in
 *  mykey must be used to unlock the door.  Note that the door can
 *  only be relocked by the player under the circumstances that allow
 *  unlocking, plus the property islockable must be set to true.
 *  By default, the door is closed; set isopen to true if the door
 *  is to start out open (and be sure to open the other side as well).
 *  
 *  otherside specifies the corresponding doorway object in the
 *  destination room (doordest), if any.  If otherside is
 *  specified, its isopen and islocked properties will be
 *  kept in sync automatically.
 */
class doorway: fixeditem, obstacle
    isdoor = true           // Item can be opened and closed
    destination =
    {
        if (self.isopen)
            return self.doordest;
        else if (not self.islocked and not self.noAutoOpen)
        {
            self.setIsopen(true);
            "(Opening << self.thedesc >>)\n";
            return self.doordest;
        }
        else
        {
            "%You%'ll have to open << self.thedesc >> first. ";
            setit(self);
            return nil;
        }
    }
    verDoOpen(actor) =
    {
        if (self.isopen)
            "\^<<self.itisdesc>> already open. ";
        else if (self.islocked)
            "\^<<self.itisdesc>> locked. ";
    }
    setIsopen(setting) =
    {
        /* update my status */
        self.isopen := setting;

        /* 
         *   if there's another side to this door, and its status is not
         *   already set to the new setting, update it as well (we don't
         *   update it if it's already been updated, since otherwise we'd
         *   recurse forever calling back and forth between the two sides) 
         */
        if (self.otherside != nil && self.otherside.isopen != setting)
            self.otherside.setIsopen(setting);
    }
    setIslocked(setting) =
    {
        /* update my status */
        self.islocked := setting;

        /* if there's another side, update it as well */
        if (self.otherside != nil && self.otherside.islocked != setting)
            self.otherside.setIslocked(setting);
    }
    doOpen(actor) =
    {
        "Opened. ";
        self.setIsopen(true);
    }
    verDoClose(actor) =
    {
        if (not self.isopen)
            "\^<<self.itisdesc>> already closed. ";
    }
    doClose(actor) =
    {
        "Closed. ";
        self.setIsopen(nil);
    }
    verDoLock(actor) =
    {
        if (self.islocked)
            "\^<<self.itisdesc>> already locked! ";
        else if (not self.islockable)
            "\^<<self.itnomdesc>> can't be locked. ";
        else if (self.isopen)
            "%You%'ll have to close <<self.itobjdesc>> first. ";
    }
    doLock(actor) =
    {
        if (self.mykey = nil)
        {
            "Locked. ";
            self.setIslocked(true);
        }
        else
            askio(withPrep);
    }
    verDoUnlock(actor) =
    {
        if (not self.islocked)
            "\^<<self.itisdesc>> not locked! ";
    }
    doUnlock(actor) =
    {
        if (self.mykey = nil)
        {
            "Unlocked. ";
            self.setIslocked(nil);
        }
        else
            askio(withPrep);
    }
    verDoLockWith(actor, io) =
    {
        if (self.islocked)
            "\^<<self.itisdesc>> already locked. ";
        else if (not self.islockable)
            "\^<<self.itnomdesc>> can't be locked. ";
        else if (self.mykey = nil)
            "%You% %do%n't need anything to lock <<self.itobjdesc>>. ";
        else if (self.isopen)
            "%You%'ll have to close <<self.itobjdesc>> first. ";
    }
    doLockWith(actor, io) =
    {
        if (io = self.mykey)
        {
            "Locked. ";
            self.setIslocked(true);
        }
        else
            "\^<<io.itnomdesc>> <<io.doesdesc>>n't fit the lock. ";
    }
    verDoUnlockWith(actor, io) =
    {
        if (not self.islocked)
            "\^<<self.itisdesc>> not locked! ";
        else if (self.mykey = nil)
            "%You% %do%n't need anything to unlock <<self.itobjdesc>>. ";
    }
    doUnlockWith(actor, io) =
    {
        if (io = self.mykey)
        {
            "Unlocked. ";
            self.setIslocked(nil);
        }
        else
            "\^<<io.itnomdesc>> <<io.doesdesc>>n't fit the lock. ";
    }
    ldesc =
    {
        if (self.isopen)
            "\^<<self.itisdesc>> open. ";
        else
        {
            if (self.islocked)
                "\^<<self.itisdesc>> closed and locked. ";
            else
                "\^<<self.itisdesc>> closed. ";
        }
    }
    verDoKnock(actor) = { }
    doKnock(actor) = { "There is no answer. "; }
    verDoEnter(actor) = { }
    doEnter(actor) =
    {
        actor.travelTo(self.destination);
    }
    doSynonym('Enter') = 'Gothrough'
;

/*
 *  lockableDoorway: doorway
 *
 *  This is just a normal doorway with the islockable and
 *  islocked properties set to true.  Fill in the other
 *  properties (otherside and doordest) as usual.  If
 *  the door has a key, set property mykey to the key object.
 */
class lockableDoorway: doorway
    islockable = true
    islocked = true
;

/*
 *  vehicle: item, nestedroom
 *
 *  This is an object that an actor can board.  An actor cannot go
 *  anywhere while on board a vehicle (except where the vehicle goes);
 *  the actor must get out first.
 */
class vehicle: item, nestedroom
    reachable = ([] + self)
    isvehicle = true
    verDoEnter(actor) = { self.verDoBoard(actor); }
    doEnter(actor) = { self.doBoard(actor); }
    verDoBoard(actor) =
    {
        if (actor.location = self)
        {
            "%You're% already <<self.statusPrep>> <<self.thedesc>>! ";
        }
        else if (actor.isCarrying(self))
        {
            "%You%'ll have to drop <<self.thedesc>> first!";
        }
    }
    doBoard(actor) =
    {
        "Okay, %you're% now <<self.statusPrep>> <<self.thedesc>>. ";
        actor.moveInto(self);
    }
    noexit =
    {
        "%You're% not going anywhere until %you% get%s% <<self.outOfPrep>> <<
            self.thedesc>>. ";
        return nil;
    }
    out = (self.location)
    verDoTake(actor) =
    {
        if (actor.isIn(self))
            "%You%'ll have to get <<self.outOfPrep>> <<self.thedesc>> first.";
        else
            pass verDoTake;
    }
    dobjGen(a, v, i, p) =
    {
        if (a.isIn(self) and v <> inspectVerb and v <> getOutVerb
            and v <> outVerb)
        {
            "%You%'ll have to get <<self.outOfPrep>> <<
                self.thedesc>> first. ";
            exit;
        }
    }
    iobjGen(a, v, d, p) =
    {
        if (a.isIn(self) and v <> putVerb)
        {
            "%You%'ll have to get <<self.outOfPrep>> <<
                self.thedesc>> first. ";
            exit;
        }
    }
;

/*
 *  surface: item
 *
 *  Objects can be placed on a surface.  Apart from using the
 *  preposition "on" rather than "in" to refer to objects
 *  contained by the object, a surface is identical to a
 *  container.  Note: an object cannot be both a
 *  surface and a container, because there is no
 *  distinction between the two internally.
 */
class surface: item
    issurface = true        // Item can hold objects on its surface
    ldesc =
    {
        if (itemcnt(self.contents))
        {
            "On "; self.thedesc; " %you% see%s% "; listcont(self); ". ";
        }
        else
        {
            "There's nothing on "; self.thedesc; ". ";
        }
    }
    verIoPutOn(actor) = {}
    ioPutOn(actor, dobj) =
    {
        dobj.doPutOn(actor, self);
    }
    verDoSearch(actor) = {}
    doSearch(actor) =
    {
        if (itemcnt(self.contents) <> 0)
            "On <<self.thedesc>> %you% see%s% <<listcont(self)>>. ";
        else
            "There's nothing on <<self.thedesc>>. ";
    }
;


/*
 *  qsurface: surface
 *
 *  This is a minor variation of the standard surface that doesn't list
 *  its contents by default when the surface itself is described.
 */
class qsurface: surface
    isqsurface = true
;


/*
 *  container: item
 *
 *  This object can contain other objects.  The iscontainer property
 *  is set to true.  The default ldesc displays a list of the
 *  objects inside the container, if any.  The maxbulk property
 *  specifies the maximum amount of bulk the container can contain.
 */
class container: item
    maxbulk = 10            // maximum bulk the container can contain
    isopen = true           // in fact, it can't be closed at all
    iscontainer = true      // Item can contain other items
    ldesc =
    {
        if (self.contentsVisible and itemcnt(self.contents) <> 0)
        {
            "In "; self.thedesc; " %you% see%s% "; listcont(self); ". ";
        }
        else
        {
            "There's nothing in "; self.thedesc; ". ";
        }
    }
    verIoPutIn(actor) =
    {
    }
    ioPutIn(actor, dobj) =
    {
        if (addbulk(self.contents) + dobj.bulk > self.maxbulk)
        {
            "%You% can't fit <<dobj.thatdesc>> in "; self.thedesc; ". ";
        }
        else
        {
            dobj.doPutIn(actor, self);
        }
    }
    verDoLookin(actor) = {}
    doLookin(actor) =
    {
        self.doSearch(actor);
    }
    verDoSearch(actor) = {}
    doSearch(actor) =
    {
        if (self.contentsVisible and itemcnt(self.contents) <> 0)
            "In <<self.thedesc>> %you% see%s% <<listcont(self)>>. ";
        else
            "There's nothing in <<self.thedesc>>. ";
    }
;

/*
 *  openable: container
 *
 *  A container that can be opened and closed.  The isopenable
 *  property is set to true.  The default ldesc displays
 *  the contents of the container if the container is open, otherwise
 *  a message saying that the object is closed.
 */
class openable: container
    contentsReachable = { return self.isopen; }
    contentsVisible = { return self.isopen; }
    isopenable = true
    ldesc =
    {
        caps(); self.thedesc; " "; self.isdesc;
        if (self.isopen)
        {
            " open. ";
            pass ldesc;
        }
        else
        {
            " closed. ";
            
            /* if it's transparent, list its contents anyway */
            if (isclass(self, transparentItem))
                pass ldesc;
        }
    }
    isopen = true
    verDoOpen(actor) =
    {
        if (self.isopen)
        {
            caps(); self.thedesc; " <<self.isdesc>> already open! ";
        }
    }
    doOpen(actor) =
    {
        if (itemcnt(self.contents))
        {
            "Opening "; self.thedesc; " reveals "; listcont(self); ". ";
        }
        else
            "Opened. ";
        self.isopen := true;
    }
    verDoClose(actor) =
    {
        if (not self.isopen)
        {
            caps(); self.thedesc; " <<self.isdesc>> already closed! ";
        }
    }
    doClose(actor) =
    {
        "Closed. ";
        self.isopen := nil;
    }
    verIoPutIn(actor) =
    {
        if (not self.isopen)
        {
            caps(); self.thedesc; " <<self.isdesc>> closed. ";
        }
    }
    verDoLookin(actor) =
    {
        /* we can look in it if either it's open or it's transparent */
        if (not self.isopen and not isclass(self, transparentItem))
           "\^<<self.itisdesc>> closed. ";
    }
    verDoSearch(actor) = { self.verDoLookin(actor); }
;

/*
 *  qcontainer: container
 *
 *  A "quiet" container:  its contents are not listed when it shows
 *  up in a room description or inventory list.  The isqcontainer
 *  property is set to true.
 */
class qcontainer: container
    isqcontainer = true
;

/*
 *  lockable: openable
 *
 *  A container that can be locked and unlocked.  The islocked
 *  property specifies whether the object can be opened or not.  The
 *  object can be locked and unlocked without the need for any other
 *  object; if you want a key to be involved, use a keyedLockable.
 */
class lockable: openable
    verDoOpen(actor) =
    {
        if (self.islocked)
        {
            "\^<<self.itisdesc>> locked. ";
        }
        else
            pass verDoOpen;
    }
    verDoLock(actor) =
    {
        if (self.islocked)
        {
            "\^<<self.itisdesc>> already locked! ";
        }
    }
    doLock(actor) =
    {
        if (self.isopen)
        {
            "%You%'ll have to close "; self.thedesc; " first. ";
        }
        else
        {
            "Locked. ";
            self.islocked := true;
        }
    }
    verDoUnlock(actor) =
    {
        if (not self.islocked)
            "\^<<self.itisdesc>> not locked! ";
    }
    doUnlock(actor) =
    {
        "Unlocked. ";
        self.islocked := nil;
    }
    verDoLockWith(actor, io) =
    {
        if (self.islocked)
            "\^<<self.itisdesc>> already locked. ";
    }
    verDoUnlockWith(actor, io) =
    {
        if (not self.islocked)
            "\^<<self.itisdesc>> not locked! ";
        else if (self.mykey = nil)
            "%You% %do%n't need anything to unlock << self.itobjdesc >>.";
    }
;

/*
 *  keyedLockable: lockable
 *
 *  This subclass of lockable allows you to create an object
 *  that can only be locked and unlocked with a corresponding key.
 *  Set the property mykey to the keyItem object that can
 *  lock and unlock the object.
 */
class keyedLockable: lockable
    mykey = nil     // set 'mykey' to the key which locks/unlocks me
    doLock(actor) =
    {
        askio(withPrep);
    }
    doUnlock(actor) =
    {
        askio(withPrep);
    }
    doLockWith(actor, io) =
    {
        if (self.isopen)
        {
            "%You% can't lock << self.thedesc >> when <<
            self.itisdesc>> open. ";
        }
        else if (io = self.mykey)
        {
            "Locked. ";
            self.islocked := true;
        }
        else
            "\^<<io.itnomdesc>> <<io.doesdesc>>n't fit the lock. ";
    }
    doUnlockWith(actor, io) =
    {
        if (io = self.mykey)
        {
            "Unlocked. ";
            self.islocked := nil;
        }
        else
            "\^<<io.itnomdesc>> <<io.doesdesc>>n't fit the lock. ";
    }
;

/*
 *  keyItem: item
 *
 *  This is an object that can be used as a key for a keyedLockable
 *  or lockableDoorway object.  It otherwise behaves as an ordinary item.
 */
class keyItem: item
    verIoUnlockWith(actor) = {}
    ioUnlockWith(actor, dobj) =
    {
        dobj.doUnlockWith(actor, self);
    }
    verIoLockWith(actor) = {}
    ioLockWith(actor, dobj) =
    {
        dobj.doLockWith(actor, self);
    }
;

/*
 *  seethruItem: item
 *
 *  This is an object that the player can look through, such as a window.
 *  The thrudesc method displays a message for when the player looks
 *  through the object (with a command such as "look through window").
 *  Note this is not the same as a transparentItem, whose contents
 *  are visible from outside the object.
 */
class seethruItem: item
    verDoLookthru(actor) = {}
    doLookthru(actor) = { self.thrudesc; }
;


/*
 *  transparentItem: item
 *
 *  An object whose contents are visible, even when the object is
 *  closed.  Whether the contents are reachable is decided in the
 *  normal fashion.  This class is useful for items such as glass
 *  bottles, whose contents can be seen when the bottle is closed
 *  but cannot be reached.
 */
class transparentItem: item
    contentsVisible = { return true; }
    ldesc =
    {
        if (self.contentsVisible and itemcnt(self.contents) <> 0)
        {
            "In "; self.thedesc; " %you% see%s% "; listcont(self); ". ";
        }
        else
        {
            "There's nothing in "; self.thedesc; ". ";
        }
    }
    verGrab(obj) =
    {
        if (self.isopenable and not self.isopen)
            "%You% will have to open << self.thedesc >> first. ";
    }
    doOpen(actor) =
    {
        self.isopen := true;
        "Opened. ";
    }
    verDoLookin(actor) = {}
    doLookin(actor) = { self.doSearch(actor); }
    verDoSearch(actor) = {}
    doSearch(actor) =
    {
        if (self.contentsVisible and itemcnt(self.contents) <> 0)
            "In <<self.thedesc>> %you% see%s% <<listcont(self)>>. ";
        else
            "There's nothing in <<self.thedesc>>. ";
    }
;

/*
 *  basicNumObj: object
 *
 *  This object provides a default implementation for numObj.
 *  To use this default unchanged in your game, include in your
 *  game this line:  "numObj: basicNumObj".
 */
class basicNumObj: object   // when a number is used in a player command,
    value = 0               //  this is set to its value
    sdesc = "<<value>>"
    adesc = "a number"
    thedesc = "the number <<value>>"
    verDoTypeOn(actor, io) = {}
    doTypeOn(actor, io) = { "\"Tap, tap, tap, tap...\" "; }
    verIoTurnTo(actor) = {}
    ioTurnTo(actor, dobj) = { dobj.doTurnTo(actor, self); }
;

/*
 *  basicStrObj: object
 *
 *  This object provides a default implementation for strObj.
 *  To use this default unchanged in your game, include in your
 *  game this line:  "strObj: basicStrObj".
 */
class basicStrObj: object   // when a string is used in a player command,
    value = ''              //  this is set to its value
    sdesc = "\"<<value>>\""
    adesc = "\"<<value>>\""
    thedesc = "\"<<value>>\""
    verDoTypeOn(actor, io) = {}
    doTypeOn(actor, io) = { "\"Tap, tap, tap, tap...\" "; }
    doSynonym('TypeOn') = 'EnterOn' 'EnterIn' 'EnterWith'
    verDoSave(actor) = {}
    saveGame(actor) =
    {
        if (save(self.value))
        {
            "Save failed. ";
            return nil;
        }
        else
        {
            "Saved. ";
            return true;
        }
    }
    doSave(actor) =
    {
        self.saveGame(actor);
        abort;
    }
    verDoRestore(actor) = {}
    restoreGame(actor) =
    {
        return mainRestore(self.value);
    }
    doRestore(actor) =
    {
        self.restoreGame(actor);
        abort;
    }
    verDoScript(actor) = {}
    startScripting(actor) =
    {
        logging(self.value);
        "Writing script file. ";
    }
    doScript(actor) =
    {
        self.startScripting(actor);
        abort;
    }
    verDoSay(actor) = {}
    doSay(actor) =
    {
        "Okay, \""; say(self.value); "\".";
    }
;

/*
 *  deepverb: object
 *
 *  A "verb object" that is referenced by the parser when the player
 *  uses an associated vocabulary word.  A deepverb contains both
 *  the vocabulary of the verb and a description of available syntax.
 *  The verb property lists the verb vocabulary words;
 *  one word (such as 'take') or a pair (such as 'pick up')
 *  can be used.  In the latter case, the second word must be a
 *  preposition, and may move to the end of the sentence in a player's
 *  command, as in "pick it up."  The action(actor)
 *  method specifies what happens when the verb is used without any
 *  objects; its absence specifies that the verb cannot be used without
 *  an object.  The doAction specifies the root of the message
 *  names (in single quotes) sent to the direct object when the verb
 *  is used with a direct object; its absence means that a single object
 *  is not allowed.  Likewise, the ioAction(preposition)
 *  specifies the root of the message name sent to the direct and
 *  indirect objects when the verb is used with both a direct and
 *  indirect object; its absence means that this syntax is illegal.
 *  Several ioAction properties may be present:  one for each
 *  preposition that can be used with an indirect object with the verb.
 *  
 *  The validDo(actor, object, seqno) method returns true
 *  if the indicated object is valid as a direct object for this actor.
 *  The validIo(actor, object, seqno) method does likewise
 *  for indirect objects.  The seqno parameter is a "sequence
 *  number," starting with 1 for the first object tried for a given
 *  verb, 2 for the second, and so forth; this parameter is normally
 *  ignored, but can be used for some special purposes.  For example,
 *  askVerb does not distinguish between objects matching vocabulary
 *  words, and therefore accepts only the first from a set of ambiguous
 *  objects.  These methods do not normally need to be changed; by
 *  default, they return true if the object is accessible to the
 *  actor.
 *  
 *  The doDefault(actor, prep, indirectObject) and
 *  ioDefault(actor, prep) methods return a list of the
 *  default direct and indirect objects, respectively.  These lists
 *  are used for determining which objects are meant by "all" and which
 *  should be used when the player command is missing an object.  These
 *  normally return a list of all objects that are applicable to the
 *  current command.
 *  
 *  The validDoList(actor, prep, indirectObject) and
 *  validIoList(actor, prep, directObject) methods return
 *  a list of all of the objects for which validDo would be true.
 *  Remember to include floating objects, which are generally
 *  accessible.  Note that the objects returned by this list will
 *  still be submitted by the parser to validDo, so it's okay for
 *  this routine to return too many objects.  In fact, this
 *  routine is entirely unnecessary; if you omit it altogether, or
 *  make it return nil, the parser will simply submit every
 *  object matching the player's vocabulary words to validDo.
 *  The reason to provide this method is that it can significantly
 *  improve parsing speed when the game has lots of objects that
 *  all have the same vocabulary word, because it cuts down on the
 *  number of times that validDo has to be called (each call
 *  to validDo is fairly time-consuming).
 */
class deepverb: object                // A deep-structure verb.
    validDo(actor, obj, seqno) =
    {
        return obj.isReachable(actor);
    }
    validDoList(actor, prep, iobj) =
    {
        local ret;
        local loc;
        
        loc := actor.location;
        while (loc.location)
            loc := loc.location;
        ret := visibleList(actor, actor) + visibleList(loc, actor)
               + global.floatingList;
        return ret;
    }
    validIo(actor, obj, seqno) =
    {
        return obj.isReachable(actor);
    }
    validIoList(actor, prep, dobj) = (self.validDoList(actor, prep, dobj))
    doDefault(actor, prep, io) =
    {
        return actor.contents + actor.location.contents;
    }
    ioDefault(actor, prep) =
    {
        return actor.contents + actor.location.contents;
    }
;

/*
   Dark verb - a verb that can be used in the dark.  Travel verbs
   are all dark verbs, as are system verbs (quit, save, etc.).
   In addition, certain special verbs are usable in the dark:  for
   example, you can drop objects you are carrying, and you can turn
   on light sources you are carrying. 
*/

class darkVerb: deepverb
   isDarkVerb = true
;

/*
 *   Various verbs.
 */
inspectVerb: deepverb
    verb = 'inspect' 'examine' 'look at' 'l at' 'x'
    sdesc = "inspect"
    doAction = 'Inspect'
    validDo(actor, obj, seqno) =
    {
        return obj.isVisible(actor);
    }
;
askVerb: deepverb
    verb = 'ask'
    sdesc = "ask"
    prepDefault = aboutPrep
    ioAction(aboutPrep) = 'AskAbout'
    validIo(actor, obj, seqno) = { return (seqno = 1); }
    validIoList(actor, prep, dobj) = (nil)
;
tellVerb: deepverb
    verb = 'tell'
    sdesc = "tell"
    prepDefault = aboutPrep
    ioAction(aboutPrep) = 'TellAbout'
    validIo(actor, obj, seqno) = { return (seqno = 1); }
    validIoList(actor, prep, dobj) = (nil)
    ioDefault(actor, prep) =
    {
        if (prep = aboutPrep)
            return [];
        else
            return (actor.contents + actor.location.contents);
    }
;
followVerb: deepverb
    sdesc = "follow"
    verb = 'follow'
    doAction = 'Follow'
;
digVerb: deepverb
    verb = 'dig' 'dig in'
    sdesc = "dig in"
    prepDefault = withPrep
    ioAction(withPrep) = 'DigWith'
;
jumpVerb: deepverb
    verb = 'jump' 'jump over' 'jump off'
    sdesc = "jump"
    doAction = 'Jump'
    action(actor) = { "Wheeee!"; }
;
pushVerb: deepverb
    verb = 'push' 'press'
    sdesc = "push"
    doAction = 'Push'
;
attachVerb: deepverb
    verb = 'attach' 'connect'
    sdesc = "attach"
    prepDefault = toPrep
    ioAction(toPrep) = 'AttachTo'
;
wearVerb: deepverb
    verb = 'wear' 'put on' 'don'
    sdesc = "wear"
    doAction = 'Wear'
;
dropVerb: deepverb, darkVerb
    verb = 'drop' 'put down'
    sdesc = "drop"
    ioAction(onPrep) = 'PutOn'
    doAction = 'Drop'
    doDefault(actor, prep, io) =
    {
        return actor.contents;
    }
;
removeVerb: deepverb
    verb = 'take off' 'doff'
    sdesc = "take off"
    doAction = 'Unwear'
    ioAction(fromPrep) = 'RemoveFrom'
;
openVerb: deepverb
    verb = 'open'
    sdesc = "open"
    doAction = 'Open'
;
closeVerb: deepverb
    verb = 'close'
    sdesc = "close"
    doAction = 'Close'
;
putVerb: deepverb
    verb = 'put' 'place'
    sdesc = "put"
    prepDefault = inPrep
    ioAction(inPrep) = 'PutIn'
    ioAction(onPrep) = 'PutOn'
    doDefault(actor, prep, io) =
    {
        return (takeVerb.doDefault(actor, prep, io) + actor.contents);
    }
;
takeVerb: deepverb                   // This object defines how to take things
    verb = 'take' 'pick up' 'get' 'remove'
    sdesc = "take"
    ioAction(offPrep) = 'TakeOff'
    ioAction(outPrep) = 'TakeOut'
    ioAction(fromPrep) = 'TakeOut'
    ioAction(inPrep) = 'TakeOut'
    ioAction(onPrep) = 'TakeOff'
    doAction = 'Take'
    doDefault(actor, prep, io) =
    {
        local ret, rem, cur, rem2, cur2, tot, i, tot2, j;
        
        ret := [];
        
        /*
         *   For "take all out/off of <iobj>", return the (non-fixed)
         *   contents of the indirect object.  Same goes for "take all in
         *   <iobj>", "take all on <iobj>", and "take all from <iobj>".
         */
        if ((prep = outPrep or prep = offPrep or prep = inPrep
             or prep = onPrep or prep = fromPrep)
            and io <> nil)
        {
            rem := io.contents;
            i := 1;
            tot := length(rem);
            while (i <= tot)
            {
                cur := rem[i];
                if (not cur.isfixed and self.validDo(actor, cur, i))
                    ret += cur;
                ++i;
            }
            return ret;
        }
        
        /*
         *   In the general case, return everything that's not fixed
         *   in the actor's location, or everything inside fixed containers
         *   that isn't itself fixed.
         */
        rem := actor.location.contents;
        tot := length(rem);
        i := 1;
        while (i <= tot)
        {
            cur := rem[i];
            if (cur.isfixed)
            {
                if (((cur.isopenable and cur.isopen) or (not cur.isopenable))
                    and (not cur.isactor))
                {
                    rem2 := cur.contents;
                    tot2 := length(rem2);
                    j := 1;
                    while (j <= tot2)
                    {
                        cur2 := rem2[j];
                        if (not cur2.isfixed and not cur2.notakeall)
                        {
                            ret := ret + cur2;
                        }
                        j := j + 1;
                    }
                }
            }
            else if (not cur.notakeall)
            {
                ret := ret + cur;
            }
            
            i := i + 1;            
        }
        return ret;
    }
;
plugVerb: deepverb
    verb = 'plug'
    sdesc = "plug"
    prepDefault = inPrep
    ioAction(inPrep) = 'PlugIn'
;
lookInVerb: deepverb
    verb = 'look in' 'look on' 'l in' 'l on'
    sdesc = "look in"
    doAction = 'Lookin'
;
screwVerb: deepverb
    verb = 'screw'
    sdesc = "screw"
    ioAction(withPrep) = 'ScrewWith'
    doAction = 'Screw'
;
unscrewVerb: deepverb
    verb = 'unscrew'
    sdesc = "unscrew"
    ioAction(withPrep) = 'UnscrewWith'
    doAction = 'Unscrew'
;
turnVerb: deepverb
    verb = 'turn' 'rotate' 'twist'
    sdesc = "turn"
    ioAction(toPrep) = 'TurnTo'
    ioAction(withPrep) = 'TurnWith'
    doAction = 'Turn'
;
switchVerb: deepverb
    verb = 'switch'
    sdesc = "switch"
    doAction = 'Switch'
;
flipVerb: deepverb
    verb = 'flip'
    sdesc = "flip"
    doAction = 'Flip'
;
turnOnVerb: deepverb, darkVerb
    verb = 'activate' 'turn on' 'switch on'
    sdesc = "turn on"
    doAction = 'Turnon'
;
turnOffVerb: deepverb
    verb = 'turn off' 'deactiv' 'switch off'
    sdesc = "turn off"
    doAction = 'Turnoff'
;
lookVerb: deepverb
    verb = 'look' 'l' 'look around' 'l around'
    sdesc = "look"
    action(actor) =
    {
        actor.location.lookAround(true);
    }
;
sitVerb: deepverb
    verb = 'sit on' 'sit in' 'sit' 'sit down' 'sit downin' 'sit downon'
    sdesc = "sit on"
    doAction = 'Siton'
;
lieVerb: deepverb
    verb = 'lie' 'lie on' 'lie in' 'lie down' 'lie downon' 'lie downin'
    sdesc = "lie on"
    doAction = 'Lieon'
;
getOutVerb: deepverb
    verb = 'get out' 'get outof' 'get off' 'get offof'
    sdesc = "get out of"
    doAction = 'Unboard'
    action(actor) = { askdo; }
    doDefault(actor, prep, io) =
    {
        if (actor.location and actor.location.location)
            return ([] + actor.location);
        else
            return [];
    }
;
boardVerb: deepverb
    verb = 'get in' 'get into' 'board' 'get on'
    sdesc = "get on"
    doAction = 'Board'
;
againVerb: darkVerb         // Required verb:  repeats last command.  No
                            // action routines are necessary; this one's
                            // handled internally by the parser.
    verb = 'again' 'g'
    sdesc = "again"
;
waitVerb: darkVerb
    verb = 'wait' 'z'
    sdesc = "wait"
    action(actor) =
    {
        "Time passes...\n";
    }
;
iVerb: deepverb
    verb = 'inventory' 'i'
    sdesc = "inventory"
    useInventoryTall = nil
    action(actor) =
    {
        if (itemcnt(actor.contents))
        {
            /* use tall or wide mode, as appropriate */
            if (self.useInventoryTall)
            {
                /* use "tall" mode */
                "%You% %are% carrying:\n";
                nestlistcont(actor, 1);
            }
            else
            {
                /* use wide mode */
                "%You% %have% "; listcont(actor); ". ";
                listcontcont(actor);
            }
        }
        else
            "%You% %are% empty-handed.\n";
    }
;
iwideVerb: deepverb
    verb = 'i_wide'
    sdesc = "inventory"
    action(actor) =
    {
        iVerb.useInventoryTall := nil;
        iVerb.action(actor);
    }
;
itallVerb: deepverb
    verb = 'i_tall'
    sdesc = "inventory"
    action(actor) =
    {
        iVerb.useInventoryTall := true;
        iVerb.action(actor);
    }
;
lookThruVerb: deepverb
    verb = 'look through' 'look thru' 'l through' 'l thru'
    sdesc = "look through"
    doAction = 'Lookthru'
;
breakVerb: deepverb
    verb = 'break' 'ruin' 'destroy'
    sdesc = "break"
    doAction = 'Break'
;
attackVerb: deepverb
    verb = 'attack' 'kill' 'hit'
    sdesc = "attack"
    prepDefault = withPrep
    ioAction(withPrep) = 'AttackWith'
;
climbVerb: deepverb
    verb = 'climb'
    sdesc = "climb"
    doAction = 'Climb'
;
eatVerb: deepverb
    verb = 'eat' 'consume'
    sdesc = "eat"
    doAction = 'Eat'
;
drinkVerb: deepverb
    verb = 'drink'
    sdesc = "drink"
    doAction = 'Drink'
;
giveVerb: deepverb
    verb = 'give' 'offer'
    sdesc = "give"
    prepDefault = toPrep
    ioAction(toPrep) = 'GiveTo'
    doDefault(actor, prep, io) =
    {
        return actor.contents;
    }
;
pullVerb: deepverb
    verb = 'pull'
    sdesc = "pull"
    doAction = 'Pull'
;
readVerb: deepverb
    verb = 'read'
    sdesc = "read"
    doAction = 'Read'
;
throwVerb: deepverb
    verb = 'throw' 'toss'
    sdesc = "throw"
    prepDefault = atPrep
    ioAction(atPrep) = 'ThrowAt'
    ioAction(toPrep) = 'ThrowTo'
;
standOnVerb: deepverb
    verb = 'stand on'
    sdesc = "stand on"
    doAction = 'Standon'
;
standVerb: deepverb
    verb = 'stand' 'stand up' 'get up'
    sdesc = "stand"
    outhideStatus = 0
    action(actor) =
    {
        if (actor.location = nil or actor.location.location = nil)
            "%You're% already standing! ";
        else
        {
            /* 
             *   silently check verDoUnboard, to see if we can get out of
             *   whatever object we're in 
             */
            self.outhideStatus := outhide(true);
            actor.location.verDoUnboard(actor);
            if (outhide(self.outhideStatus))
            {
                /* 
                 *   verification failed - run again, showing the message
                 *   this time 
                 */
                actor.location.verDoUnboard(actor);
            }
            else
            {
                /* verification succeeded - unboard */
                actor.location.doUnboard(actor);
            }
        }
    }
;
helloVerb: deepverb
    verb = 'hello' 'hi' 'greetings'
    sdesc = "hello"
    action(actor) =
    {
        "Nice weather we've been having.\n";
    }
;
showVerb: deepverb
    verb = 'show'
    sdesc = "show"
    prepDefault = toPrep
    ioAction(toPrep) = 'ShowTo'
    doDefault(actor, prep, io) =
    {
        return actor.contents;
    }
;
cleanVerb: deepverb
    verb = 'clean'
    sdesc = "clean"
    ioAction(withPrep) = 'CleanWith'
    doAction = 'Clean'
;
sayVerb: deepverb
    verb = 'say'
    sdesc = "say"
    doAction = 'Say'
;
yellVerb: deepverb
    verb = 'yell' 'shout' 'yell at' 'shout at'
    sdesc = "yell"
    action(actor) =
    {
        "%Your% throat is a bit sore now. ";
    }
;
moveVerb: deepverb
    verb = 'move'
    sdesc = "move"
    ioAction(withPrep) = 'MoveWith'
    ioAction(toPrep) = 'MoveTo'
    doAction = 'Move'
;
fastenVerb: deepverb
    verb = 'fasten' 'buckle' 'buckle up'
    sdesc = "fasten"
    doAction = 'Fasten'
;
unfastenVerb: deepverb
    verb = 'unfasten' 'unbuckle'
    sdesc = "unfasten"
    doAction = 'Unfasten'
;
unplugVerb: deepverb
    verb = 'unplug'
    sdesc = "unplug"
    ioAction(fromPrep) = 'UnplugFrom'
    doAction = 'Unplug'
;
lookUnderVerb: deepverb
    verb = 'look under' 'look beneath' 'l under' 'l beneath'
    sdesc = "look under"
    doAction = 'Lookunder'
;
lookBehindVerb: deepverb
    verb = 'look behind' 'l behind'
    sdesc = "look behind"
    doAction = 'Lookbehind'
;
typeVerb: deepverb
    verb = 'type'
    sdesc = "type"
    prepDefault = onPrep
    ioAction(onPrep) = 'TypeOn'
;
lockVerb: deepverb
    verb = 'lock'
    sdesc = "lock"
    ioAction(withPrep) = 'LockWith'
    doAction = 'Lock'
    prepDefault = withPrep
;
unlockVerb: deepverb
    verb = 'unlock'
    sdesc = "unlock"
    ioAction(withPrep) = 'UnlockWith'
    doAction = 'Unlock'
    prepDefault = withPrep
;
detachVerb: deepverb
    verb = 'detach' 'disconnect'
    prepDefault = fromPrep
    ioAction(fromPrep) = 'DetachFrom'
    doAction = 'Detach'
    sdesc = "detach"
;
sleepVerb: darkVerb
    action(actor) =
    {
        if (actor.cantSleep)
            "%You% %are% much too anxious worrying about %your% continued
            survival to fall asleep now. ";
        else if (global.awakeTime+1 < global.sleepTime)
            "%You're% not tired. ";
        else if (not (actor.location.isbed or actor.location.ischair))
            "I don't know about you, but I can never sleep
            standing up. %You% should find a nice comfortable
            bed somewhere. ";
        else
        {
            "%You% quickly drift%s% off into dreamland...\b";
            goToSleep();
        }
    }
    verb = 'sleep'
    sdesc = "sleep"
;
pokeVerb: deepverb
    verb = 'poke' 'jab'
    sdesc = "poke"
    doAction = 'Poke'
;
touchVerb: deepverb
    verb = 'touch'
    sdesc = "touch"
    doAction = 'Touch'
;
moveNVerb: deepverb
    verb = 'move north' 'move n' 'push north' 'push n'
    sdesc = "move north"
    doAction = 'MoveN'
;
moveSVerb: deepverb
    verb = 'move south' 'move s' 'push south' 'push s'
    sdesc = "move south"
    doAction = 'MoveS'
;
moveEVerb: deepverb
    verb = 'move east' 'move e' 'push east' 'push e'
    sdesc = "move east"
    doAction = 'MoveE'
;
moveWVerb: deepverb
    verb = 'move west' 'move w' 'push west' 'push w'
    sdesc = "move west"
    doAction = 'MoveW'
;
moveNEVerb: deepverb
    verb = 'move northeast' 'move ne' 'push northeast' 'push ne'
    sdesc = "move northeast"
    doAction = 'MoveNE'
;
moveNWVerb: deepverb
    verb = 'move northwest' 'move nw' 'push northwest' 'push nw'
    sdesc = "move northwest"
    doAction = 'MoveNW'
;
moveSEVerb: deepverb
    verb = 'move southeast' 'move se' 'push southeast' 'push se'
    sdesc = "move southeast"
    doAction = 'MoveSE'
;
moveSWVerb: deepverb
    verb = 'move southwest' 'move sw' 'push southwest' 'push sw'
    sdesc = "move southwest"
    doAction = 'MoveSW'
;
centerVerb: deepverb
    verb = 'center'
    sdesc = "center"
    doAction = 'Center'
;
searchVerb: deepverb
    verb = 'search'
    sdesc = "search"
    doAction = 'Search'
;
knockVerb: deepverb
    verb = 'knock' 'knock on' 'knock at'
    sdesc = "knock"
    doAction = 'Knock'
;

/*
 *   Travel verbs  - these verbs allow the player to move about.
 *   All travel verbs have the property isTravelVerb set true.
 */
class travelVerb: deepverb, darkVerb
    isTravelVerb = true
;

eVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'e' 'east' 'go east'
    sdesc = "go east"
    travelDir(actor) = { return actor.location.(actor.eastProp); }
;
sVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 's' 'south' 'go south'
    sdesc = "go south"
    travelDir(actor) = { return actor.location.(actor.southProp); }
;
nVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'n' 'north' 'go north'
    sdesc = "go north"
    travelDir(actor) = { return actor.location.(actor.northProp); }
;
wVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'w' 'west' 'go west'
    sdesc = "go west"
    travelDir(actor) = { return actor.location.(actor.westProp); }
;
neVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'ne' 'northeast' 'go ne' 'go northeast'
    sdesc = "go northeast"
    travelDir(actor) = { return actor.location.(actor.neProp); }
;
nwVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'nw' 'northwest' 'go nw' 'go northwest'
    sdesc = "go northwest"
    travelDir(actor) = { return actor.location.(actor.nwProp); }
;
seVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'se' 'southeast' 'go se' 'go southeast'
    sdesc = "go southeast"
    travelDir(actor) = { return actor.location.(actor.seProp); }
;
swVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'sw' 'southwest' 'go sw' 'go southwest'
    sdesc = "go southwest"
    travelDir(actor) = { return actor.location.(actor.swProp); }
;
inVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'in' 'go in' 'enter' 'go to' 'go into'
    sdesc = "enter"
    doAction = 'Enter'
    travelDir(actor) = { return actor.location.(actor.inProp); }
    ioAction(onPrep) = 'EnterOn'
    ioAction(inPrep) = 'EnterIn'
    ioAction(withPrep) = 'EnterWith'
;
outVerb: travelVerb
    sdesc = "go out"
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'out' 'go out' 'exit' 'leave'
    travelDir(actor) = { return actor.location.(actor.outProp); }
;
dVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'd' 'down' 'go down'
    sdesc = "go down"
    travelDir(actor) = { return actor.location.(actor.downProp); }
;
uVerb: travelVerb
    action(actor) = { actor.travelTo(self.travelDir(actor)); }
    verb = 'u' 'up' 'go up'
    sdesc = "go up"
    travelDir(actor) = { return actor.location.(actor.upProp); }
;

gothroughVerb: deepverb
    verb = 'go through' 'go thru'
    sdesc = "go through"
    doAction = 'Gothrough'
;

/*
 *   sysverb:  A system verb.  Verbs of this class are special verbs that
 *   can be executed without certain normal validations.  For example,
 *   a system verb can be executed in a dark room.  System verbs are
 *   for operations such as saving, restoring, and quitting, which are
 *   not really part of the game.
 */
class sysverb: deepverb, darkVerb
    issysverb = true
;

quitVerb: sysverb
    verb = 'quit' 'q'
    sdesc = "quit"
    quitGame(actor) =
    {
        local yesno;

        scoreRank();
        "\bDo you really want to quit? (YES or NO) > ";
        yesno := yorn();
        "\b";
        if (yesno = 1)
        {
            terminate();    // allow user good-bye message
            quit();
        }
        else
        {
            "Okay. ";
        }
    }
    action(actor) =
    {
        self.quitGame(actor);
        abort;
    }
;
verboseVerb: sysverb
    verb = 'verbose'
    sdesc = "verbose"
    verboseMode(actor) =
    {
        "Okay, now in VERBOSE mode.\n";
        global.verbose := true;
        parserGetMe().location.lookAround(true);
    }
    action(actor) =
    {
        self.verboseMode(actor);
        abort;
    }
;
terseVerb: sysverb
    verb = 'brief' 'terse'
    sdesc = "terse"
    terseMode(actor) =
    {
        "Okay, now in TERSE mode.\n";
        global.verbose := nil;
    }
    action(actor) =
    {
        self.terseMode(actor);
        abort;
    }
;
scoreVerb: sysverb
    verb = 'score' 'status'
    sdesc = "score"
    showScore(actor) =
    {
        scoreRank();
    }
    action(actor) =
    {
        self.showScore(actor);
        abort;
    }
;
saveVerb: sysverb
    verb = 'save'
    sdesc = "save"
    doAction = 'Save'
    saveGame(actor) =
    {
        local savefile;
        
        savefile := askfile('File to save game in',
                            ASKFILE_PROMPT_SAVE, FILE_TYPE_SAVE,
                            ASKFILE_EXT_RESULT);
        switch(savefile[1])
        {
        case ASKFILE_SUCCESS:
            if (save(savefile[2]))
            {
                "An error occurred saving the game position to the file. ";
                return nil;
            }
            else
            {
                "Saved. ";
                return true;
            }

        case ASKFILE_CANCEL:
            "Canceled. ";
            return nil;
            
        case ASKFILE_FAILURE:
        default:
            "Failed. ";
            return nil;
        }
    }
    action(actor) =
    {
        self.saveGame(actor);
        abort;
    }
;
restoreVerb: sysverb
    verb = 'restore'
    sdesc = "restore"
    doAction = 'Restore'
    restoreGame(actor) =
    {
        local savefile;
        
        savefile := askfile('File to restore game from',
                            ASKFILE_PROMPT_OPEN, FILE_TYPE_SAVE,
                            ASKFILE_EXT_RESULT);
        switch(savefile[1])
        {
        case ASKFILE_SUCCESS:
            return mainRestore(savefile[2]);

        case ASKFILE_CANCEL:
            "Canceled. ";
            return nil;

        case ASKFILE_FAILURE:
        default:
            "Failed. ";
            return nil;
        }
    }
    action(actor) =
    {
        self.restoreGame(actor);
        abort;
    }
;

scriptVerb: sysverb
    verb = 'script'
    sdesc = "script"
    doAction = 'Script'
    startScripting(actor) =
    {
        local scriptfile;
        
        scriptfile := askfile('File to write transcript to',
                              ASKFILE_PROMPT_SAVE, FILE_TYPE_LOG,
                              ASKFILE_EXT_RESULT);

        switch(scriptfile[1])
        {
        case ASKFILE_SUCCESS:
            logging(scriptfile[2]);
            "All text will now be saved to the script file.
            Type UNSCRIPT at any time to discontinue scripting.";
            break;

        case ASKFILE_CANCEL:
            "Canceled. ";
            break;

        case ASKFILE_FAILURE:
            "Failed. ";
            break;
        }
    }
    action(actor) =
    {
        self.startScripting(actor);
        abort;
    }
;
unscriptVerb: sysverb
    verb = 'unscript'
    sdesc = "unscript"
    stopScripting(actor) =
    {
        logging(nil);
        "Script closed.\n";
    }
    action(actor) =
    {
        self.stopScripting(actor);
        abort;
    }
;
restartVerb: sysverb
    verb = 'restart'
    sdesc = "restart"
    restartGame(actor) =
    {
        local yesno;
        while (true)
        {
            "Are you sure you want to start over? (YES or NO) > ";
            yesno := yorn();
            if (yesno = 1)
            {
                "\n";
                scoreStatus(0, 0);
                restart(initRestart, global.initRestartParam);
                break;
            }
            else if (yesno = 0)
            {
                "\nOkay.\n";
                break;
            }
        }
    }
    action(actor) =
    {
        self.restartGame(actor);
        abort;
    }
;
versionVerb: sysverb
    verb = 'version'
    sdesc = "version"
    showVersion(actor) =
    {
        version.sdesc;
    }
    action(actor) =
    {
        self.showVersion(actor);
        abort;
    }
;
debugVerb: sysverb
    verb = 'debug'
    sdesc = "debug"
    enterDebugger(actor) =
    {
        if (debugTrace())
            "You can't think this game has any bugs left in it... ";
    }
    action(actor) =
    {
        self.enterDebugger(actor);
        abort;
    }
;

undoVerb: sysverb
    verb = 'undo'
    sdesc = "undo"
    undoMove(actor) =
    {
        /* do TWO undo's - one for this 'undo', one for previous command */
        if (undo() and undo())
        {
            "(Undoing one command)\b";
            parserGetMe().location.lookAround(true);
            scoreStatus(global.score, global.turnsofar);
        }
        else
            "No more undo information is available. ";
    }
    action(actor) =
    {
        self.undoMove(actor);
        abort;
    }
;

/*
 *  Prep: object
 *
 *  A preposition.  The preposition property specifies the
 *  vocabulary word.
 */
class Prep: object
;

/*
 *   Various prepositions
 */
ofPrep: Prep
    preposition = 'of'
    sdesc = "of"
;
aboutPrep: Prep
    preposition = 'about'
    sdesc = "about"
;
withPrep: Prep
    preposition = 'with'
    sdesc = "with"
;
toPrep: Prep
    preposition = 'to'
    sdesc = "to"
;
onPrep: Prep
    preposition = 'on' 'onto' 'downon' 'upon'
    sdesc = "on"
;
inPrep: Prep
    preposition = 'in' 'into' 'downin'
    sdesc = "in"
;
offPrep: Prep
    preposition = 'off' 'offof'
    sdesc = "off"
;
outPrep: Prep
    preposition = 'out' 'outof'
    sdesc = "out"
;
fromPrep: Prep
    preposition = 'from'
    sdesc = "from"
;
betweenPrep: Prep
    preposition = 'between' 'inbetween'
    sdesc = "between"
;
overPrep: Prep
    preposition = 'over'
    sdesc = "over"
;
atPrep: Prep
    preposition = 'at'
    sdesc = "at"
;
aroundPrep: Prep
    preposition = 'around'
    sdesc = "around"
;
thruPrep: Prep
    preposition = 'through' 'thru'
    sdesc = "through"
;
dirPrep: Prep
    preposition = 'north' 'south' 'east' 'west' 'up' 'down' 'northeast' 'ne'
                  'northwest' 'nw' 'southeast' 'se' 'southwest' 'sw'
                  'n' 's' 'e' 'w' 'u' 'd'
    sdesc = "north"         // Shouldn't ever need this, but just in case
;
underPrep: Prep
    preposition = 'under' 'beneath'
    sdesc = "under"
;
behindPrep: Prep
    preposition = 'behind'
    sdesc = "behind"
;

/*
 *   articles:  the "built-in" articles.  "The," "a," and "an" are
 *   defined.
 */
articles: object
    article = 'the' 'a' 'an'
;

