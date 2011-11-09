#include <tads.h>

Book template 'bookName';
Watch template 'watchName';
Item template 'itemName';
object template 'name';

Book 'book 1';
book2: Book 'book2';
Watch 'watch 1';
watch2: Watch 'watch 2';
Stopwatch 'stopwatch 1';
stopwatch2: Stopwatch 'stopwatch 2';
Item 'item 1';
item2: Item 'item 2';
Thing 'thing 1';
thing2: Thing name='thing 2';


class Book: Item
    sdesc = "bookName = <<bookName>>, itemName = <<itemName>>\n"
    bookName = '(bookName undefined)'
;

class Watch: Item
    sdesc = "watchName = <<watchName>>, itemName = <<itemName>>\n"
    watchName = '(watchName undefined)'
;

class Stopwatch: Watch
    sdesc = "stopwatch: <<inherited.sdesc>>"
;

class Item: object
    sdesc = "itemName = <<itemName>>\n"
    itemName = '(itemName undefined)'
;

class Thing: object
    name = '(name undefined)'
;

modify Thing
    sdesc = "name = <<name>>\n"
;

main(args)
{
    for (local obj = firstObj(Thing) ; obj != nil ; obj = nextObj(obj, Thing))
        obj.sdesc;
    for (local obj = firstObj(Item) ; obj != nil ; obj = nextObj(obj, Item))
        obj.sdesc;
}

