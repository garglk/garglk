/*
 *   Complex assignments test.  This tests side effects in the lvalue of a
 *   compound assignment, to make sure that each lvalue subexpression is
 *   only evaluated once and that the results are properly assigned.  
 */
#include <tads.h>
#include <strbuf.h>

obj: object
    p1 = ['v', 'w', 'x', 'y', 'z']
    p2 = static [p1, p1, p1, p1, p1]
;

obj2: object
    p1 = obj
;

main(args)
{
    local i = 1, j = 1, k = 1;
    local l = ['a', 'b', 'c', 'd', 'e'];
    local l2 = [['a', 'b', 'c', 'd', 'e'],
                ['f', 'g', 'h', 'i', 'j'],
                ['k', 'l', 'm', 'n', 'o'],
                ['p', 'q', 'r', 's', 't'],
                ['u', 'v', 'w', 'x', 'y']];
    local l3 = [l2, l2, l2, l2, l2];

    l[i++] = 'A1';
    "i=<<i>>, j=<<j>>, k=<<k>>, l = <<l.myjoin()>>\b";

    l[i++] += 'B2';
    "i=<<i>>, j=<<j>>, k=<<k>>, l = <<l.myjoin()>>\b";

    l[++i] += 'C3';
    "i=<<i>>, j=<<j>>, k=<<k>>, l = <<l.myjoin()>>\b";

    i = j = k = 1;

    l2[i++][j++] = 'D4';
    "i=<<i>>, j=<<j>>, k=<<k>>, l2 = <<l2.myjoin()>>\b";

    l2[i++][j++] += 'E5';
    "i=<<i>>, j=<<j>>, k=<<k>>, l2 = <<l2.myjoin()>>\b";

    i = j = i = 1;
    "\b";

    l3[i++][j++][k++] = 'F6';
    "i=<<i>>, j=<<j>>, k=<<k>>, l3 = <<l3.myjoin()>>\b";

    l3[i++][j++][k++] += 'G7';
    "i=<<i>>, j=<<j>>, k=<<k>>, l3 = <<l3.myjoin()>>\b";

    i = j = k = 1;

    obj.p1[i++] = 'M1';
    "i=<<i>>, j=<<j>>, k=<<k>>, obj.p1 = <<obj.p1.myjoin()>>\b";

    obj.p1[i++] += 'N2';
    "i=<<i>>, j=<<j>>, k=<<k>>, obj.p1 = <<obj.p1.myjoin()>>\b";

    i = j = k = 1;

    obj.p2[i++][j++] = 'O3';
    "i=<<i>>, j=<<j>>, k=<<k>>, obj.p2 = <<obj.p2.myjoin()>>\b";

    obj.p2[i++][j++] += 'P4';
    "i=<<i>>, j=<<j>>, k=<<k>>, obj.p2 = <<obj.p2.myjoin()>>\b";

    local a = obj;
    i = j = k = 1;
    a.p2[i++][j++] = 'Q5';
    "i=<<i>>, j=<<j>>, k=<<k>>, obj.p2 = <<obj.p2.myjoin()>>\b";

    local b = &p2;
    a.(b)[i++][j++] += 'R6';
    "i=<<i>>, j=<<j>>, k=<<k>>, obj.p2 = <<obj.p2.myjoin()>>\b";

    func1(obj2, 'obj2').(func1(&p1, 'p1')).(func1(&p2, 'p2'))
      [func1(3, 'idx 3')][func1(4, 'idx 4')] += func1('S8', 'val S8');
    "i=<<i>>, j=<<j>>, k=<<k>>, obj.p2 = <<obj.p2.myjoin()>>\b";
}

func1(x, desc)
{
    "&rarr; This is func1(<<desc>>)!\n";
    return x;
}

modify List
    myjoin(depth = 0)
    {
        local s = new StringBuffer();
        s.append('[');

        for (local i = 1, local len = length() ; i <= len ; ++i)
        {
            if (i > 1)
                s.append(', ');
            
            local ele = self[i];
            if (dataType(ele) == TypeList)
                s.append(ele.myjoin(depth + 1));
            else
                s.append(ele);
        }

        s.append(']');
        return toString(s);
    }
;
