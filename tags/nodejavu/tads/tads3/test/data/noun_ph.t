/*
 *   Simple "noun phrase" parser test.  This isn't meant to be a real noun
 *   phrase parser; it's just a demonstration of how some of the
 *   dictionary object's features work.
 */

#include "tads.h"
#include "t3.h"
#include "dict.h"

/* define our dictionary */
dictionary G_dict;

/* define our dictionary properties */
dictionary property noun, adjective, plural;

/* ------------------------------------------------------------------------ */
/*
 *   define a few objects 
 */

class Item: object
;

class Book: Item
    noun = 'book'
    plural = 'books'
    sdesc = "book"
;

class Ball: Item
    noun = 'ball'
    plural = 'balls'
    sdesc = "ball"
;

redBook: Book
    adjective = 'red'
    sdesc = "red book"
;

blueBook: Book
    adjective = 'blue'
    sdesc = "blue book"
;

redBall: Ball
    adjective = 'red'
    sdesc = "red ball"
;

greenBall: Ball
    adjective = 'green'
    sdesc = "green ball"
;



/* ------------------------------------------------------------------------ */
/*
 *   Main routine - read and parse strings 
 */
main(args)
{
    "Welcome to the noun phrase test!  Type QUIT or Q to stop.  The
    defined objects are:\n";
    for (local obj = firstObj(Item) ; obj != nil ; obj = nextObj(obj, Item))
        "\t<<obj.sdesc>>\n";
    
    for (;;)
    {
        local str;
        local toklist;
        local objlist;

        /* read a string and convert it to miniscules */
        "\b>";
        str = inputLine().toLower();

        /* tokenize it */
        toklist = tokenize(str);

        /* if tokenizing failed, ignore this input and continue */
        if (toklist == nil)
            continue;

        /* check for a QUIT command */
        if (toklist.length() == 1
            && toklist[1] == 'quit' || toklist[1] == 'q')
            break;

        /* parse the noun phrase to get an object list */
        objlist = parseNounPhrase(toklist);

        /* display the result */
        if (objlist != nil)
        {
            if (objlist.length() > 0)
            {
                "Matching objects:\n";
                for (local i = 1, local len = objlist.length() ;
                     i <= len ; ++i)
                    "\t<<i>>: <<objlist[i].sdesc>>\n";
            }
            else
                "No matching objects.\n";
        }
    }
}

/*
 *   A simple tokenizer 
 */
tokenize(str)
{
    local toks = ['[a-z][-\'a-z0-9]*', '[.,?!;:]'];
    local i, len;
    local result;

    /* start with an empty result list */
    result = [];

    /* keep going until we exhaust the string */
scanLoop:
    while (str != '')
    {
        local matchLen;

        /* skip any leading spaces */
        matchLen = rexMatch(' +', str);
        if (matchLen != nil)
        {
            /* skip the leading spaces */
            str = str.substr(matchLen + 1);

            /* if that leaves us with nothing, we're done */
            if (str == '')
                break;
        }
        
        /* compare the string to our various token patterns */
        for (i = 1, len = toks.length() ; i <= len ; ++i)
        {
            /* check for a match */
            matchLen = rexMatch(toks[i], str);
            if (matchLen != nil)
            {
                /* 
                 *   it's a match - add the matching string to the result
                 *   list 
                 */
                result += str.substr(1, matchLen);

                /* consume these characters from the match */
                str = str.substr(matchLen + 1);

                /* continue with the main scanning loop */
                continue scanLoop;
            }
        }

        /* 
         *   this isn't a valid token - display an error and return 'nil'
         *   to indicate failure 
         */
        "'<<str.substr(1, 1)>>' is not a valid character. ";
        return nil;
    }

    /* we're done - return the result list */
    return result;
}

/*
 *   A simple noun phrase parser and resolver 
 */
parseNounPhrase(toklist)
{
    local i;
    local start;
    local len;
    local result;
    
    /* start parsing at the first word */
    start = 1;

    /* if the first word is an article, skip it */
    if (toklist.length() >= 1
        && rexMatch('^(the|a|an)$', toklist[start]) != nil)
    {
        /* it's an article - skip it */
        ++start;
    }

    /* 
     *   Find all of the objects matching the given list of words.  Treat
     *   every word but the last as an adjective, and the last as a noun. 
     */
    for (i = start, len = toklist.length() ; i <= len ; ++i)
    {
        local prop;
        local curResult;

        /* 
         *   if this is the last word, treat it as a noun; otherwise,
         *   treat it as an adjective 
         */
        prop = (i == len ? &noun : &adjective);

        /* look it up in the dictionary */
        curResult = G_dict.findWord(toklist[i], prop);
        curResult += G_dict.findWordTrunc(toklist[i], prop);

        /* if we don't know the word, say so, and give up */
        if (curResult == [])
        {
            /* check to see if the word is defined under any property */
            if (!G_dict.isWordDefined(toklist[i])
                && !G_dict.isWordDefinedTrunc(toklist[i]))
            {
                /* the word simply isn't defined */
                "I don't know the word \"<<toklist[i]>>.\" ";
                return nil;
            }
            else
            {
                /* the word is defined, but not for this part of speech */
                "You don't see any ";
                for (i = start ; i <= toklist.length() ; ++i)
                    "<<toklist[i]>> ";
                "here. ";
                return nil;
            }
        }

        /* 
         *   if this is the first word, this is the whole list; otherwise,
         *   intersect the list so far with the new list, since we only
         *   want objects that define every single word in the input
         *   phrase
         */
        if (i == start)
            result = curResult;
        else
            result = result.intersect(curResult);
    }

    /* return the result list */
    return result;
}

