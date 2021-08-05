/*
 *   vocabulary test
 */

#include "tads.h"
#include "t3.h"
#include "dict.h"

dictionary G_dict;
dictionary property noun, adjective, plural;

modify Book
    noun = 'chapter'
    replace adjective = 'ancient'
;

blueBook: Book
    adjective = 'blue'
    sdesc = "blue_book"
;

class SwitchItem: Item
    noun = 'switch'
;

