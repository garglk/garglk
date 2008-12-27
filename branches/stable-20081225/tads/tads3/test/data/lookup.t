#include "tads.h"
#include "lookup.h"

#define show(x) showTab(tab, #@x, x)

showTab(tab, caption, val)
{
    "<<caption>> -> <<tab[val]>>\n";
}

main(args)
{
    local tab = new LookupTable(16, 32);

    /* insert some elements */
    tab['hello'] = 1;
    tab['goodbye'] = 2;
    tab['asdf'] = 3;
    tab[[1,2]] = 4;
    tab[[1,3]] = 5;
    tab[2.818] = 6;
    tab[3.1415] = 7;

    /* show the values */
    show('hello');
    show('goodbye');
    show('asdf');
    show('asdf2');
    show([1,2]);
    show([1,3]);
    show([2,3]);
    show(2.818);
    show(2.818000);
    show(0.28180e1);
    show(3.1415);
    show(1.707);

    /* change some values */
    tab['goodbye'] *= 100;
    tab[[1,3]] *= 100;

    /* show the changes */
    "\bafter changes:\n";
    show('hello');
    show('goodbye');
    show('asdf');
    show('asdf2');
    show([1,2]);
    show([1,3]);
    show([2,3]);
    show(2.818);
    show(3.1415);
    show(1.707);

    /* remove some keys */
    tab.removeElement('hello');
    tab.removeElement([1,2]);
    
    /* show the changes */
    "\bafter removals:\n";
    show('hello');
    show('goodbye');
    show('asdf');
    show('asdf2');
    show([1,2]);
    show([1,3]);
    show([2,3]);
    show(2.818);
    show(3.1415);
    show(1.707);

    /* iterate through the table */
    "\bIterating:\n";
    foreach (local val in tab)
        "\t<<val>>\n";

    /* iterate again, this time using the forEach method */
    "\bAgain, using tab.forEach:\n";
    tab.forEachAssoc({k,v: "\t[<<sayval(k)>>] = <<v>>\n"});

    /* try an iterator */
    "\bIterator:\n";
    for (local iter = tab.createIterator() ; iter.isNextAvailable() ; )
    {
        local cur;
        
        /* get the next element */
        cur = iter.getNext();

        /* show it, and the current key/value from the iterator */
        "\tgetNext() returned <<cur>>, cur key =
        <<sayval(iter.getCurKey())>>,  cur val = <<iter.getCurVal()>>\n";
    }
}

sayval(val)
{
    if (dataType(val) == TypeList)
    {
        local first = true;
        "[";
        foreach (local x in val)
        {
            if (!first)
                ",";
            first = nil;
            sayval(x);
        }
        "]";
    }
    else
        tadsSay(val);
}

