#include <tads.h>
#include <bignum.h>
#include <strbuf.h>
#include <bytearr.h>


main(args)
{
    testNew('A4/(A10)', 'One', 'Two', 'Three', 'Four', 'Five');
    testNew('C/l', [0x12345678, 0x11aabbcc]);
    testNew('A10 X5 A3', 'aaaaaaaaaa', 'BBBBB');
    testNew('b*', 'byte array test!');

    local arr = new ByteArray(1000);
    local idx = 1;
    idx += arr.packBytes(idx, 'C/l', 1, 2, 3, 4, 5);
    idx += arr.packBytes(idx, 'C/(a0)', 'one', 'two', 'three', 'four', 'five');
    idx += arr.packBytes(idx, '[A5 C]3!', ['abc', 1], ['def', 2], ['ghijkl']);
    "pack in place\n";
    ".. pack hex (full array) = <<arr.unpackBytes(1, 'H*')[1]>>\n";
    ".. pack hex (up to idx = <<idx-1>>) = <<
      arr.unpackBytes(1, 'H!' + (idx-1))[1]>>\n";

    idx = 1;
    local lst = arr.unpackBytes(idx, 'C/l @?');
    ".. unpack 1 = <<sayList(lst)>>\n";

    idx += lst[lst.length()];
    lst = arr.unpackBytes(idx, 'C/(a0) @?');
    ".. unpack 2 = <<sayList(lst)>>\n";

    idx += lst[lst.length()];
    lst = arr.unpackBytes(idx, '[A5 C]3!');
    ".. unpack 3 = <<sayList(lst)>>\n";
}

testNew(fmt, [args])
{
    local arr = ByteArray.packBytes(fmt, args...);
    "format = <<fmt.htmlify()>>, args = <<sayList(args)>>\n";
    ".. pack hex = <<arr.unpackBytes(1, 'H*')[1]>>\n";
    ".. unpack = <<sayList(arr.unpackBytes(1, fmt))>>\b";
}

sayList(lst)
{
    "[";
    
    local idx = 0;
    foreach (local val in lst)
    {
        if (idx++ > 0)
            ", ";

        switch (dataType(val))
        {
        case TypeList:
            sayList(val);
            break;

        case TypeSString:
            "'<<quoteStr(val)>>'";
            break;

        case TypeInt:
            "<<val>> (0x<<toString(val, 16)>>)";
            break;

        case TypeObject:
            if (val.ofKind(BigNumber))
            {
                "<<val>> (bignum)";
                if (val.getWhole() == val && val.getScale() <= 20)
                    " (0x<<bignumToHex(val)>>)";
            }
            else if (val.ofKind(ByteArray))
            {
                "'<<quoteStr(toString(val))>>' (ByteArray)";
            }
            else
                "(object)";
            break;
            
        default:
            "<<val>>";
            break;
        }
    }
    
    "]";
}

quoteStr(str)
{
    return rexReplace('[\'\\\n\t\b]', str, new function(m)
    {
        switch (m)
        {
        case '\'': return '\\\'';
        case '\\': return '\\\\';
        case '\n': return '\\n';
        case '\r': return '\\r';
        case '\t': return '\\t';
        case '\b': return '\\b';
        case '\^': return '\\^';
        case '\v': return '\\v';
        case '\ ': return '\\\ ';
        default:
            m = toString(m.toUnicode(1), 16);
            return m.length() < 2 ? '0x0<<m>>' : '0x<<m>>';
        }
    });
}

bignumToHex(val)
{
    local s = new StringBuffer(30);
    local neg = (val < 0);
    if (neg)
        val = -val;

    while (val > 0)
    {
        local q = val.divideBy(16);
        s.append(toString(toInteger(q[2]), 16));
        val = q[1];
    }

    if (s.length() == 0)
        s.append('0');
    if (neg)
        s.append('-');

    for (local i = 1, local j = s.length() ; i < j ; ++i, --j)
    {
        local tmp = s[i];
        s[i] = s[j];
        s[j] = tmp;
    }

    return toString(s);
}
