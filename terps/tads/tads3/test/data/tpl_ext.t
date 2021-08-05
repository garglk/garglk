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
thing2: Thing 'thing 2';
Box 'box 1';
box2: Box 'box 2';

modify Thing
    sdesc = "name = <<name>>\n"
;

modify Watch
    sdesc = "modified watch sdesc: <<inherited.sdesc>>"
;

#if 0
extern class Foo;
Foo 'foo 1';
foo2: Foo 'foo 2';
#endif

main(args)
{
    for (local obj = firstObj(Thing) ; obj != nil ; obj = nextObj(obj, Thing))
        obj.sdesc;
    for (local obj = firstObj(Item) ; obj != nil ; obj = nextObj(obj, Item))
        obj.sdesc;
}

