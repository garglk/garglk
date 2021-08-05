#include "tads.h"
#include "t3.h"

preinit() { }

main(argc)
{
    local x;
    local y;
    local lst, str;

    x = [1, 2, 3].length();
    "length([1,2,3])=<<x>>\n";

    lst = [4, 5, 6];
    x = lst.length();

    lst += [7, 8];
    x = lst.length();

    lst = lst.splice(1, 0, 1, 2, 3);
    "lst=<<sayList(lst)>>:\n";
    "sublist(2, 3) = <<sayList(lst.sublist(2, 3))>>\n";
    "sublist(6, 10) = <<sayList(lst.sublist(6, 10))>>\n";
    "sublist(7) = <<sayList(lst.sublist(7))>>\n";
    "sublist(-4, 2) = <<sayList(lst.sublist(-4, 2))>>\n";
    "sublist(-3) = <<sayList(lst.sublist(-3))>>\n";
    "sublist(1, -1) = <<sayList(lst.sublist(1, -1))>>\n";
    "sublist(2, -2) = <<sayList(lst.sublist(2, -2))>>\n";
    "sublist(2, -1) = <<sayList(lst.sublist(2, -1))>>\n";
    "sublist(2, -2) = <<sayList(lst.sublist(2, -2))>>\n";
    "sublist(2, -5) = <<sayList(lst.sublist(2, -5))>>\n";
    "sublist(5, -5) = <<sayList(lst.sublist(5, -5))>>\n";
    "sublist(6, -6) = <<sayList(lst.sublist(6, -6))>>\n";
    "sublist(12, -2) = <<sayList(lst.sublist(12, -2))>>\n";
    "sublist(2, -12) = <<sayList(lst.sublist(2, -12))>>\n";
    "sublist(1, -9) = <<sayList(lst.sublist(1, -9))>>\n";
    "sublist(1, -8) = <<sayList(lst.sublist(1, -8))>>\n";
    "sublist(1, -7) = <<sayList(lst.sublist(1, -7))>>\n";
    "sublist(1, -6) = <<sayList(lst.sublist(1, -6))>>\n";

    "\b";
    "[1, 2, 3, 4, 5, 6], keep>3 =
        <<sayList([1,2,3,4,5,6].subset({x: x > 3}))>>\n";

    lst = [6, 5, 4, 3, 2, 1];
    "lst = <<sayList(lst)>>, keep>3 =
        <<sayList(lst.subset({x: x > 3}))>>\n";

    lst += [9, 0, 10, -1, 11, -2];
    "lst = <<sayList(lst)>>, keep>3 =
        <<sayList(lst.subset({x: x > 3}))>>\n";

    "lst = <<sayList(lst)>>, *100 =
        <<sayList(lst.mapAll({x: x*100}))>>\n";

    lst += lst;
    "\bConcatenating list to itself: <<sayList(lst)>>\n";
    "\b";

    "forEach: "; lst.forEach(new function(x) { "<<x>>; "; }); "\n";
    "\b";

    "indexOf 6: <<lst.indexOf(6)>>\n";
    "indexOf -2: <<lst.indexOf(-2)>>\n";
    "indexOf 9: <<lst.indexOf(9)>>\n";
    "indexOf 123: <<lst.indexOf(123)>>\n";
    "\b";

    "indexWhich x > 0:  <<lst.indexWhich({x: x > 0})>>\n";
    "indexWhich x > 7:  <<lst.indexWhich({x: x > 7})>>\n";
    "indexWhich x &lt; 0:  <<lst.indexWhich({x: x < 0})>>\n";
    "indexWhich x > 1000:  <<lst.indexWhich({x: x > 1000})>>\n";
    "\b";

    "valWhich x > 0: <<lst.valWhich({x: x > 0})>>\n";
    "valWhich x > 7: <<lst.valWhich({x: x > 7})>>\n";
    "valWhich x &lt; 0: <<lst.valWhich({x: x < 0})>>\n";
    "valWhich x &lt; 4: <<lst.valWhich({x: x < 4})>>\n";
    "valWhich x > 2000: <<lst.valWhich({x: x > 2000})>>\n";
    "\b";

    "lastIndexOf 6: <<lst.lastIndexOf(6)>>\n";
    "lastIndexOf -2: <<lst.lastIndexOf(-2)>>\n";
    "lastIndexOf 9: <<lst.lastIndexOf(9)>>\n";
    "lastIndexOf 123: <<lst.lastIndexOf(123)>>\n";
    "\b";

    "lastIndexWhich x > 0: <<lst.lastIndexWhich({x: x > 0})>>\n";
    "lastIndexWhich x &lt; 0: <<lst.lastIndexWhich({x: x < 0})>>\n";
    "lastIndexWhich x > 10: <<lst.lastIndexWhich({x: x > 10})>>\n";
    "lastIndexWhich x > 2000: <<lst.lastIndexWhich({x: x > 2000})>>\n";
    "\b";

    "lastValWhich x > 0: <<lst.lastValWhich({x: x > 0})>>\n";
    "lastValWhich x &lt; 0: <<lst.lastValWhich({x: x < 0})>>\n";
    "lastValWhich x > 500: <<lst.lastValWhich({x: x > 500})>>\n";
    "lastValWhich x > 2000: <<lst.lastValWhich({x: x > 2000})>>\n";
    "\b";

    "countOf 6: <<lst.countOf(6)>>\n";
    "countOf 5: <<lst.countOf(5)>>\n";
    "countOf -2: <<lst.countOf(-2)>>\n";
    "countOf 123: <<lst.countOf(123)>>\n";
    "\b";

    "countWhich x > 0: <<lst.countWhich({x: x > 0})>>\n";
    "countWhich x > 2000: <<lst.countWhich({x: x > 2000})>>\n";
    "countWhich x &lt; 2000: <<lst.countWhich({x: x < 2000})>>\n";
    "\b";

    "getUnique: <<sayList(lst.getUnique())>>\n";
    "getUnique [5, 1, 5, 2, 5, 5, 2, 7]:
     <<sayList([5, 1, 5, 2, 5, 5, 2, 7].getUnique())>>\n";
    "\b";

    lst = [1, 2, 3, 4, 5];
    "appendUnique: <<sayList(lst.appendUnique([2, 4, 6, 8, 10, 12]))>>\n";

    lst += 6;
    "appendUnique: <<sayList(lst.appendUnique([3, 4, 5, 6, 7]))>>\n";

    y = [1, 3, 5];
    y += [3, 5];
    "appendUnique: <<sayList(lst.appendUnique(y))>>\n";

    y += [9, 11];
    "appendUnique: <<sayList(lst.appendUnique(y))>>\n";

    "\b";
    "append 77: <<sayList(lst.append(77))>>\n";
    "append [11,22,33]: <<sayList(lst.append([11,22,33]))>>\n";

    "\b";
    y = [11, 20, 1, 17, 5, 3];
    y += [9, 8, 7, 6];
    "sort: <<sayList(y.sort())>>\n";
    "sort descending: <<sayList(y.sort(true))>>\n";
    "sort in string ordering:
    <<sayList(y.sort(nil,
    {a, b: toString(a) > toString(b)
    ? 1 : toString(a) < toString(b) ? -1 : 0}))>>\n";

    "\b";

    y = [1, 2, 3, 4, 5];
    y = y.prepend(77);
    y = y.prepend(88);
    y = y.prepend(99);
    "prepend: <<sayList(y)>>\n";

    "\b";

    y = [10, 20, 30, 40, 50];
    y = y.insertAt(1, 1, 2, 3);
    "insert1: <<sayList(y)>>\n";

    y = y.insertAt(9, 44, 55, 66);
    "insert2: <<sayList(y)>>\n";
    
    y = y.insertAt(5, 777, 888, 999);
    "insert3: <<sayList(y)>>\n";

    y = y.insertAt(-2, 1234, 5678);
    "insert4: <<sayList(y)>>\n";

    y = y.removeElementAt(-4).removeElementAt(-3);
    "undid insert4: <<sayList(y)>>\n";

    "\b";

    y = y.removeElementAt(6);
    "removeAt(6): <<sayList(y)>>\n";

    y = y.removeElementAt(1);
    "removeAt(1): <<sayList(y)>>\n";

    y = y.removeElementAt(y.length());
    "removeAt(length): <<sayList(y)>>\n";

    y = y.removeElementAt(-2);
    "removeAt(-2): <<sayList(y)>>\n";

    y = y.insertAt(-1, 44);
    "undid removeAt(-2): <<sayList(y)>>\n";

    "\b";

    y = y.removeRange(3, 7);
    "removeRange(3, 7): <<sayList(y)>>\n";

    y = y.removeRange(1, 2);
    "removeRange(1, 2): <<sayList(y)>>\n";

    y = y.removeRange(y.length() - 1, y.length());
    "removeRange(length-1, length): <<sayList(y)>>\n";

    y = List.generate({i: i*10}, 10);
    y = y.removeRange(-3, -2);
    "removeRange(-3, -2): <<sayList(y)>>\n";

    "\b";
    x = List.generate({i: makeString(96+i)}, 10);
    "x=<<sayList(x)>>:\n";
    "splice(3, 2, 'x', 'y')=<<sayList(x.splice(3, 2, 'x', 'y'))>>\n";
    "splice(3, 5, 'x', 'y')=<<sayList(x.splice(3, 5, 'x', 'y'))>>\n";
    "splice(7, 10, 'x', 'y')=<<sayList(x.splice(7, 10, 'x', 'y'))>>\n";
    "splice(0, 5, 'x', 'y')=<<sayList(x.splice(0, 5, 'x', 'y'))>>\n";
    "splice(-2, 0, 'x', 'y')=<<sayList(x.splice(-2, 0, 'x', 'y'))>>\n";

    "\b";
    "'goodbye', len = <<'goodbye'.length()>>\n";

    str = 'hello';
    "str = '<<str>>', len = <<str.length()>>\n";

    str += '!!!';
    "str = '<<str>>', len = <<str.length()>>\n";

    "'goodbye', substr(5) = <<'goodbye'.substr(5)>>\n";
    "'goodbye', substr(4, 2) = <<'goodbye'.substr(4, 2)>>\n";

    str = 'hello there';
    "str = '<<str>>', substr(5) = <<str.substr(5)>>\n";
    "str = '<<str>>', substr(4, 2) = <<str.substr(4, 2)>>\n";

    str += '!!!';
    "str = '<<str>>', substr(5) = <<str.substr(5)>>\n";
    "str = '<<str>>', substr(4, 2) = <<str.substr(4, 2)>>\n";
}

sayList(lst)
{
    if (lst == nil)
        "nil";
    else
    {
        "[";
        for (local i = 1, local cnt = lst.length() ; i <= cnt ; ++i)
        {
            if (i != 1)
                ", ";
            if (dataType(lst[i]) == TypeList)
                sayList(lst[i]);
            else
                tadsSay(lst[i]);
        }
        "]";
    }
}
