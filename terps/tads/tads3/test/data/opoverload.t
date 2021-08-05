#include <tads.h>

class Thing: object
    operator +(b) { return 'this is ' + name + ' + ' + nameof(b); }
    operator -(b) { return 'this is ' + name + ' - ' + nameof(b); }
    operator *(b) { return 'this is ' + name + ' * ' + nameof(b); }
    operator /(b) { return 'this is ' + name + ' / ' + nameof(b); }
    operator %(b) { return 'this is ' + name + ' % ' + nameof(b); }
    operator ^(b) { return 'this is ' + name + ' ^ ' + nameof(b); }
    operator <<(b) { return 'this is ' + name + ' &lt;&lt; ' + nameof(b); }
    operator >>(b) { return 'this is ' + name + ' &gt;&gt; ' + nameof(b); }
    operator >>>(b)
        { return 'this is ' + name + ' &gt;&gt;&gt; ' + nameof(b); }
    operator ~() { return 'this is ~' + name; }
    operator &(b) { return 'this is ' + name + ' & ' + nameof(b); }
    operator |(b) { return 'this is ' + name + ' | ' + nameof(b); }
    operator negate() { return 'this is -' + name; }

    operator [](i) { return 'this is ' + name + '[' + nameof(i) + ']'; }
    operator []=(i, val)
    {
        " {{in <<name>>[<<nameof(i)>>] = <<nameof(val)>>}} ";
        return 'updated a with [' + nameof(i) + '] set to '
            + nameof(val) + '!';
    }

    construct(n) { name = n; }
    name = 'Thing'

    getSelf() { return self; }
;

class Other: object
    operator +(b) { return 'Other+'; }
    operator -(b) { return 'Other-'; }
    operator ~ = 7
;

modify String
    operator [](i) { return toUnicode(i); }
    operator []=(i, ch)
    {
        if (dataType(ch) == TypeInt)
            ch = makeString(ch);
        return substr(1, i-1) + ch + substr(i+1);
    }
;

modify List
    show()
    {
        "[";
        for (local i = 1 ; i <= length() ; ++i)
        {
            if (i > 1)
                ", ";
            "<<self[i]>>";
        }
        "]";
    }
;

modify Vector
    show()
    {
        "[";
        for (local i = 1 ; i <= length() ; ++i)
        {
            if (i > 1)
                ", ";
            "<<self[i]>>";
        }
        "]";
    }
;

class ReverseList: object
    construct(lst) { self.lst = lst; }
    operator [](i) { return lst[lst.length() + 1 - i]; }
    length() { return lst.length(); }

    lst = []
;

nameof(i)
{
    switch (dataType(i))
    {
    case TypeObject:
        return i.name;

    case TypeInt:
        return toString(i);

    case TypeSString:
        return i;

    default:
        return '?unknown?';
    }
}

yesno(x)
{
    "<<x ? 'yes' : 'no'>>";
}

main(args)
{
    local a, b;

    "Baseline tests - integer values\n";
    a = 123;
    b = 7;

    "a+b = <<a+b>>\n";
    "a-b = <<a-b>>\n";
    "a*b = <<a*b>>\n";
    "a/b = <<a/b>>\n";
    "a%b = <<a%b>>\n";
    "a&b = <<a&b>>\n";
    "a|b = <<a|b>>\n";
    "a^b = <<a^b>>\n";
    "a&lt;&lt;b = <<a<<b>>\n";
    "a&gt;&gt;b = <<(a>>b)>>\n";
    "a&gt;&gt;&gt;b = <<(a>>>b)>>\n";
    "~a = <<~a>>\n";
    "-a = <<-a>>\n";

    "\bOperator overload tests - regular objects\n";
    a = new Thing('a');
    b = new Thing('b');

    "\b";
    "a+1 = <<a+1>>\n";
    "a+b = <<a+b>>\n";
    "a-1 = <<a-1>>\n";
    "a-b = <<a-b>>\n";
    "a*b = <<a*b>>\n";
    "a/b = <<a/b>>\n";
    "a%b = <<a%b>>\n";
    "a&b = <<a&b>>\n";
    "a|b = <<a|b>>\n";
    "a^b = <<a^b>>\n";
    "a&lt;&lt;b = <<a<<b>>\n";
    "a&gt;&gt;b = <<(a>>b)>>\n";
    "a&gt;&gt;&gt;b = <<(a>>>b)>>\n";
    "~a = <<~a>>\n";
    "-a = <<-a>>\n";
    "a[b] = <<a[b]>>\n";
    "a.getSelf[b] = <<a.getSelf()[b]>>\n";
    "a[19] = <<a[19]>>\n";

    local aOrig = a;
    a[b] = 'hello';
    "a = <<a>>\n";

    a = aOrig;
    a[7] = 'hello';
    "a[7]='hello': a = <<a>>\n";

    a = aOrig;
    a += b;
    "a+=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a += 1;
    "a+=1: a=<<nameof(a)>>\n";

    a = aOrig;
    a += 7;
    "a+=7: a=<<nameof(a)>>\n";

    a = aOrig;
    a += 12345;
    "a+=12345: a=<<nameof(a)>>\n";

    a = aOrig;
    a -= b;
    "a-=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a -= 1;
    "a-=1: a=<<nameof(a)>>\n";

    a = aOrig;
    a *= b;
    "a*=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a /= b;
    "a/=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a %= b;
    "a%=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a |= b;
    "a|=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a &= b;
    "a&=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a ^= b;
    "a^=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a <<= b;
    "a&lt&lt;=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a >>= b;
    "a&gt;&gt;=b: a=<<nameof(a)>>\n";

    a = aOrig;
    a >>>= b;
    "a&gt;&gt;&gt;=b: a=<<nameof(a)>>\n";

    "\bOperator overload tests - String intrinsics\n";
    a = 'hello there';
    "a[1] = <<a[1]>>, a[2]=<<a[2]>>, a[3]=<<a[3]>>\n";
    a[1] = 'H';
    a[6] = '-';
    a[11] = 'E';
    "a = <<a>>\n";

    "\b'&amp;' property name tests\n";

    a = aOrig;
    "(Thing)\n";
    "a.propDefined(&amp;operator +)? <<yesno(a.propDefined(&operator +))>>\n";
    "a.propDefined(&amp;operator -)? <<yesno(a.propDefined(&operator -))>>\n";
    "a.propDefined(&amp;operator *)? <<yesno(a.propDefined(&operator *))>>\n";
    "a.propDefined(&amp;operator [])? <<
      yesno(a.propDefined(&operator []))>>\n";
    "a.propDefined(&amp;operator []=)? <<
      yesno(a.propDefined(&operator []=))>>\n";

    a = 'String';
    "(String)\n";
    "a.propDefined(&amp;operator +)? <<yesno(a.propDefined(&operator +))>>\n";
    "a.propDefined(&amp;operator -)? <<yesno(a.propDefined(&operator -))>>\n";
    "a.propDefined(&amp;operator *)? <<yesno(a.propDefined(&operator *))>>\n";
    "a.propDefined(&amp;operator [])? <<
      yesno(a.propDefined(&operator []))>>\n";
    "a.propDefined(&amp;operator []=)? <<
      yesno(a.propDefined(&operator []=))>>\n";

    a = new Other();
    "(Other)\n";
    "~a = <<~a>>\n";
    "a.propDefined(&amp;operator +)? <<yesno(a.propDefined(&operator +))>>\n";
    "a.propDefined(&amp;operator -)? <<yesno(a.propDefined(&operator -))>>\n";
    "a.propDefined(&amp;operator *)? <<yesno(a.propDefined(&operator *))>>\n";
    "a.propDefined(&amp;operator [])? <<
      yesno(a.propDefined(&operator []))>>\n";
    "a.propDefined(&amp;operator []=)? <<
      yesno(a.propDefined(&operator []=))>>\n";

    "\bReverseList tests\n";
    a = [1, 2, 3, 4, 5];
    b = new ReverseList([100, 2, 300, 4, 500, 6, 700, 8]);
    "a+b = <<(a+b).show()>>\n";
    "a-b = <<(a-b).show()>>\n";
    "a.intersect(b) = <<a.intersect(b).show()>>\n";
    "a.appendUnique(b) = <<a.appendUnique(b).show()>>\n";

    "\b...with Vectors\n";
    a = new Vector(b);
    "new Vector(b) = <<a.show()>>\n";

    a.appendAll(b);
    "a.appendAll(b) = <<a.show()>>\n";

    a = new Vector(b);
    a.appendUnique(b);
    "a.appendUnique(b) = <<a.show()>>\n";
}


