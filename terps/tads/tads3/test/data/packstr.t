#include <tads.h>
#include <bignum.h>
#include <strbuf.h>

main(args)
{
    test('A4/(A10)', 'One', 'Two', 'Three', 'Four', 'Five');
    test('C/l', [0x12345678, 0x11aabbcc]);
    test('A10 X5 A3', 'aaaaaaaaaa', 'BBBBB');

    test('c4*');
    test('c4*', 1, 2);
    test('c4*', 1, 2, 3);
    test('c4*', 1, 2, 3, 4);
    test('c4*', 1, 2, 3, 4, 5, 6);
    test('c4*', []);
    test('c4*', [1, 2, 3]);
    test('c4*', [1, 2, 3, 4]);
    test('c4*', [1, 2, 3, 4, 5]);
    test('[cc]4*!');
    test('[cc]4*!', [1, 2], [3], [4, 5]);
    test('[cc]4*!', [1, 2], [3], [4, 5], [6, 7]);
    test('[cc]4*!', [1, 2], [3], [4, 5], [6, 7], [8]);
    test('a10*', 'short');
    test('a10*', '10 exactly');
    test('a10*', 'a bit too long');

    ovTest('c', 127, 128, 129, -127, -128, -129);
    ovTest('C', 127, 128, 129, 255, 256, -1);
    ovTest('s', 32767, 32768, -32767, -32768, -32769);
    ovTest('l', 0x7fffffff, 0x80000000, 3000000000.0);
    ovTest('L~', 0x7fffffff, -1, 2.0.raiseToPower(32)-1, 2.0.raiseToPower(32));
    ovTest('q~', 0x7fffffff, 2.0.raiseToPower(63)-1, 2.0.raiseToPower(63),
           -(2.0.raiseToPower(63)), -(2.0.raiseToPower(63)+1));
    ovTest('Q~', 0x7fffffff, 2.0.raiseToPower(63)-1, 2.0.raiseToPower(63),
           -(2.0.raiseToPower(63)), -(2.0.raiseToPower(63)+1),
           2.0.raiseToPower(64)-1, 2.0.raiseToPower(64),
           2.0.raiseToPower(64) + 0x12345678);
    ovTest('f', 1.12345e38, 3.402823e38, 3.402824e38, 4.0e39,
           -3.402824e38, -3.402824e38, -4.0e39);
    ovTest('d', 1.12345e308, 1.7976931348623158e308,
           1.797694e308, -1.797694e308, 4.0e309, -4.0e309);

    local s = 'This is a test hash message!'.sha256();
    "sha256 = <<s>>\n";
    "unpacked = <<binstr(String.packBytes('H*', s))>>\b";
}

test(fmt, [args])
{
    local s = String.packBytes(fmt, args...);
    "format = <<fmt.htmlify()>>, args = <<sayList(args)>>\n";
    ".. pack = '<<binstr(s)>>'\n";
    ".. pack hex = <<s.unpackBytes('H*')[1]>>\n";
    ".. unpack = <<sayList(s.unpackBytes(fmt))>>\b";
}

ovTest(fmt, [args])
{
    foreach (local arg in args)
    {
        ovTest1(fmt, arg);
        ovTest1(fmt + '%', arg);
        "\b";
    }
}

ovTest1(fmt, arg)
{
    try
    {
        local s = String.packBytes(fmt, arg);
        local lst = s.unpackBytes(fmt);
        "pack OK: pack('<<fmt.htmlify()>>', <<arg>>) = <<
          s.unpackBytes('H*')[1]>>\n.. unpack = <<
          sayList(lst)>><<
          lst[1] != arg ? " *** value change ***\n" : "\n">>";
    }
    catch (Exception exc)
    {
        "pack error: pack('<<fmt.htmlify()>>', <<arg>>) = <<
          exc.displayException()>>\n";
    }
}

binstr(s)
{
    return rexReplace('[\x00-\x1f\\ ]', s, new function(m) {
        if (m == '\\')
            return '\\\\';
        else if (m == '\'')
            return '\\\'';
        else if (m == ' ')
            return '\ ';
        else
        {
            m = toString(m.toUnicode(1), 16);
            return '\\x<<'00'.substr(m.length() + 1)>><<m>>';
        }
    });
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
            "'<<rexReplace('[\'\\\n\t\b]', val, new function(m) {
                return ['\'' -> '\\\'', '\\' -> '\\\\',
                    '\n' -> '\\n', '\t' -> '\\t', '\b' -> '\\b',
                        * -> '\\?'][m];
                })>>'";
            break;

        case TypeInt:
            "<<val>> (0x<<toString(val, 16)>>)";
            break;

        case TypeObject:
            if (val.ofKind(BigNumber))
            {
                "<<val>> (bignum)";
                if ((val.numType() & NumTypeNum) != 0
                    && val.getWhole() == val
                    && val.getScale() <= 20)
                    " (0x<<bignumToHex(val)>>)";
            }
            else if (val.ofKind(ByteArray))
            {
                "<<val.mapToString(new CharacterSet('latin1'))>>";
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
