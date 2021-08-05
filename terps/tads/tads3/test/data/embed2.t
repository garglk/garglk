#include <tads.h>

class TestObj: object
    strprop = 'String property with embeddings: p1=<<p1>>, p2=<<p2>>'
    p1 = '<<name>>.p1'
    p2 = '<<name>>.p2'
;

obj1: TestObj
    name = 'obj1'
;
obj2: TestObj
    name = 'obj2'
;

main(args)
{
    local i = 1024;
    local j = 3;
    "This is a dstring with embeddings: i&gt;&gt;j = <<(i >> j)>>\n";

    "Here's one with embeddings and comments: <<
      /* a comment in a string */
      ((i >> j) + (i >> j))
      /* done with the comments */
    >>.  That should do it!\n";

    "\b";

    local s = 'Here\'s an sstring with embeddings: i&gt;&gt;j = <<(i >> j)>>!';
    i = 123;
    j = 456;
    "\b<<s>>\n";

    s = 'Once again: i=<<i>>, j=<<j>>';
    i = 999;
    j = 777;
    "<<s>>\n";

    s = 'And at last: i=<<i>>, j=<<j>>';
    i = j = nil;
    "<<s>>\n";

    "\b";

    "obj1.strprop = <<obj1.strprop>>\n";
    "obj2.strprop = <<obj2.strprop>>\n";
}
