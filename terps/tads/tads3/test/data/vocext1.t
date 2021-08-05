/*
 *   vocabulary test
 */

#include "tads.h"
#include "t3.h"
#include "dict.h"

dictionary G_dict;
dictionary property noun, adjective, plural;

class Item: object
;

class Book: Item
    noun = 'book' 'booklet' 'volume'
    plural = 'books' 'booklets' 'volumes'
    sdesc = "book"
    adjective = 'musty'
;

class LightItem: Item
    noun = 'light'
;

modify Book
    noun = 'tome'
    replace plural = 'tomes'
;

redBook: Book
    adjective = 'red'
    sdesc = "red_book"
;

lightSwitch: SwitchItem, LightItem
    sdesc = "light_switch"
;

main(args)
{
    "noun 'book': <<sayList(G_dict.findWord('book', &noun))>>\n";
    "noun 'booklet': <<sayList(G_dict.findWord('booklet', &noun))>>\n";
    "plural 'books': <<sayList(G_dict.findWord('books', &plural))>>\n";
    "noun 'tome': <<sayList(G_dict.findWord('tome', &noun))>>\n";
    "plural 'tomes': <<sayList(G_dict.findWord('tomes', &plural))>>\n";
    "adjective 'red': <<sayList(G_dict.findWord('red', &adjective))>>\n";
    "adjective 'musty': <<sayList(G_dict.findWord('musty', &adjective))>>\n";
    "\b";

    "redBook.noun: <<sayStrList(redBook.noun)>>\n";
    "blueBook.noun: <<sayStrList(blueBook.noun)>>\n";
    "blueBook.plural: <<sayStrList(blueBook.plural)>>\n";
    "redBook.adjective: <<sayStrList(redBook.adjective)>>\n";
    "blueBook.adjective: <<sayStrList(blueBook.adjective)>>\n";
    "blueBook.plural: <<sayStrList(blueBook.plural)>>\n";
    "\b";

    "noun 'switch': <<sayList(G_dict.findWord('switch', &noun))>>\n";
    "noun 'light': <<sayList(G_dict.findWord('light', &noun))>>\n";
    "lightSwitch.noun: <<sayStrList(lightSwitch.noun)>>\n";
    "lightSwitch.adjective: <<sayStrList(lightSwitch.adjective)>>\n";
    "\b";

    "Iterating over each dictionary entry...\n";
    G_dict.forEachWord({ obj, str, prop:
                       "\t<<str>>: <<obj.sdesc>> 
                       <<reflectionServices.valToSymbol(prop)>>\n"});
}

sayList(lst)
{
    if (lst == nil)
    {
        "nil";
    }
    else
    {
        "[";
        for (local i = 1, local len = lst.length() ; i <= len ; i += 2)
        {
            if (i > 1)
                ", ";
            lst[i].sdesc;
        }
        "]";
    }
}

sayStrList(lst)
{
    if (lst == nil)
    {
        "nil";
    }
    else
    {
        "[";
        for (local i = 1, local len = lst.length() ; i <= len ; ++i)
        {
            if (i > 1)
                ", ";
            tadsSay(lst[i]);
        }
        "]";
    }
}

