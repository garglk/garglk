#include "tads.h"
#include "t3.h"
#include "dict.h"

/* ------------------------------------------------------------------------ */
/*
 *   Default Dictionary 
 */
dictionary gDict;

dictionary property noun, adjective, plural;


/* ------------------------------------------------------------------------ */
/* 
 *   set the location property 
 */
+ property location;


/* ------------------------------------------------------------------------ */
/*
 *   define some object templates
 */
object template 'name_';
object template 'name_' "sdesc";
object template 'name_' "sdesc" "ldesc";
object template "sdesc" "ldesc";
object template [lst];

object template @location 'name_';
object template @location 'name_' "sdesc";
object template @location 'name_' "sdesc" "ldesc";


/* ------------------------------------------------------------------------ */
/*
 *   Main Entrypoint 
 */
main(args)
{
    "Containment graph:\n";
    forEachInstance(Room,
        { obj: "Room: <<obj.sdesc>><<obj.listCont(1)>>\b" });

    "Long Descriptions:\n";
    forEachInstance(CObject, { obj: "\(<<obj.sdesc>>:\) <<obj.ldesc>>\b" });
}


/* ------------------------------------------------------------------------ */
/*
 *   preinitialization 
 */
PreinitObject
    execute()
    {
        /* initialize the location for every object */
        forEachInstance(CObject, { obj: obj.initLocation() });
        
        /* initialize vocabulary */
        forEachInstance(CObject, { obj: obj.initVocabFromName(gDict) });
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   root object 
 */
class CObject: object
    /* list the contents */
    listCont(level)
    {
        for (local i = 1, local cnt = contents.length() ; i <= cnt ; ++i)
        {
            /* indent */
            for (local j = 1 ; j <= level ; ++j)
                "\ \ ";

            /* show this object and end the line */
            contents[i].sdesc;
            "\n";

            /* show the contents of this object */
            contents[i].listCont(level + 1);
        }
    }

    /* initialize location - add myself to my location's contents list */
    initLocation()
    {
        if (self.propType(&location) == TypeObject)
            location.contents += self;
    }

    /* initialize vocabulary from the 'name_' property */
    initVocabFromName(dict)
    {
        local str;
        
        /* if 'name' isn't defined for this object, ignore it */
        if (!self.propDefined(&name_))
            return;

        /* add empty lists for our undefined dictionary properties */
        if (!self.propDefined(&noun))
            noun = [];
        if (!self.propDefined(&adjective))
            adjective = [];
        if (!self.propDefined(&plural))
            plural = [];

        /* parse the name */
        for (str = name_ ; str != '' ; )
        {
            local len;
            local grp;
                
            /* 
             *   parse all of the adjectives in the string - an adjective
             *   is a leading word followed by another word 
             */
            for (;;)
            {
                /* check for another adjective */
                if ((len = rexMatch('((%w|[-\'])+) +%w', str)) == nil)
                {
                    /* no more adjectives in this part - stop looking */
                    break;
                }

                /* add this adjective */
                grp = rexGroup(1);
                adjective += grp[3];
                dict.addWord(self, grp[3], &adjective);

                /* 
                 *   remove it from the string - note that our expression
                 *   has one extra character that we don't use (the first
                 *   character of the next word), so we must deduct one
                 *   from our length; then we must add one to get the
                 *   index of the start of the next word 
                 */
                str = str.substr(len - 1 + 1);
            }

            /* 
             *   pull out the next word, which goes up to the '/', '(', or
             *   the end of the string 
             */
            if ((len = rexMatch('((%w|[-\'])+) *([/(]|$)', str)) == nil)
                break;

            /* add this word to the nouns */
            grp = rexGroup(1);
            noun += grp[3];
            dict.addWord(self, grp[3], &noun);

            /* remove it from the string */
            str = str.substr(rexGroup(3)[1]);

            /* if the string is empty now, we're done */
            if (str == '')
                break;

            /* if we have a slash, remove it and move on */
            if ((len = rexMatch('/ *', str)) != nil)
            {
                /* remove the slash and following spaces */
                str = str.substr(len + 1);
            }

            /* if the string is now empty, we're done */
            if (str == '')
                break;

            /* check for a paren group, indicating a plural */
            if ((len = rexMatch('%((.+)%)', str)) != nil)
            {
                local pstr;
                
                /* get the plural section */
                pstr = rexGroup(1)[3];

                /* remove the plurals from the rest of the string */
                str = str.substr(len + 1);

                /* scan the plurals */
                while (pstr != '')
                {
                    /* get the next word */
                    if ((len = rexMatch('((%w|[-\'])+)( +|$)', pstr)) == nil)
                        break;

                    /* add it to the plurals */
                    grp = rexGroup(1);
                    plural += grp[3];
                    dict.addWord(self, grp[3], &plural);
                }
            }
        }

        /* if we didn't empty out the string, show an error */
        if (str != '')
            "warning: <<sdesc>>.name_ is invalid from '<<str>>'\n";
    }

    /* contents list */
    contents = []
;
    
class Item: CObject
    sdesc = "<item>"
    ldesc = "It looks like an ordinary <<sdesc>>. "
;

class Container: Item
    capacity = 10
;

class Room: CObject
    sdesc = "<Room>"
;

/* ------------------------------------------------------------------------ */
/*
 *   define some objects 
 */


object sdesc = "anonymous base object" ;
+ object sdesc = "contained anonymous base object" ;

iceCave: Room "Ice Cave"
    "A glacier extends into this cavern from the north, completely
    blocking any exits save the one to the south.  Piles of pulverized
    rock lie at the front of the glacier. "
;

Item @iceCave 'glacier' "glacier";
+ Item 'pulverized pile/rock/piles/rocks' "pile of rocks" ;

nsPassage: Room "North-South Passage"
    "This narrow, twisting passage leads north and south. "
;

+ Item 'ice pick' "ice pick";
+ Container 'backpack' "backpack"
    capacity = 100
;
+ Container 'bag' "bag" ;

