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

redBook: Item
    noun = 'book' 'booklet' 'volume'
    plural = 'books' 'booklets' 'volumes'
    adjective = 'red'
;

