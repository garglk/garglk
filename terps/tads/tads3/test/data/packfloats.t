#charset "latin1"

/*
 *   File.packBytes floating point type tests
 */

#include <tads.h>
#include <file.h>
#include <bignum.h>
#include <bytearr.h>
#include <strbuf.h>


main(args)
{
    local fname = 'packfloats.bin';
    writeTest(fname);
    readTest(fname);
}

writeTest(fname)
{
    local fp = File.openRawFile(fname, FileAccessWrite);

    twrite(fp, 'd',
           1.0 + 2.2204460492503131e-016,
           222.22222222, 333.333333333333333, 444.444,
           555.555, 666.666, 777.777, 888.888, 999.999,
           2000.0002);

    // sub-normal numbers
    twrite(fp, 'd',
           1.2250738585072011e-308, 6.329835701939e-309);

    // FLT_EPSILON, FLT_MAX, FLT_MIN, 1+FLT_EPSILON, FLT_MIN minus epsilon
    twrite(fp, 'f',
           [1.192092896e-07, 3.402823466e+38, 1.175494351e-38,
            1.0 + 1.192092896e-07, 1.075494347e-38]);
    
    // DBL_EPSILON, DBL_MAX, DBL_MIN, 1+DBL_EPSILON, DBL_MIN minus epsilon
    twrite(fp, 'd',
           [2.2204460492503131e-016, 1.7976931348623158e+308,
            2.2250738585072014e-308,
            777.777,
            888.888,
            1.0 + 2.2204460492503131e-016,
            1.2250738585072011e-308]);
    
    fp.closeFile();
}

twrite(fp, fmt, [args])
{
    fp.packBytes('C/a', '{{{<<fmt>>}}}');
    fp.packBytes('C/<<fmt>>', args...);
}

readTest(fname)
{
    local fp = File.openRawFile(fname, FileAccessRead);
    local fileSize = fp.getFileSize();

    while (fp.getPos() < fileSize)
    {
        try
        {
            tread(fp);
        }
        catch (Exception exc)
        {
            "Error: <<exc.displayException()>>\n";
            break;
        }
    }

    fp.setPos(0);
    sayList(fp.unpackBytes('b*'));

    fp.closeFile();
}

tread(fp)
{
    local fmt = fp.unpackBytes('C/a')[1];
    rexMatch('%{%{%{(.*)%}%}%}', fmt);
    fmt = rexGroup(1)[3];
    "Format code = <<fmt>> ";

    fmt = 'C/(<<fmt>> X:<<fmt>> H:<<fmt>>!)';
    local lst = fp.unpackBytes(fmt);
    sayList(lst);
    "\n";
}

tab(depth)
{
    "<<makeString('\t', depth)>>";
}

sayList(lst, depth = 1)
{
    "<<tab(depth)>>[\n";
    foreach (local val in lst)
    {
        tab(depth);
        switch (dataType(val))
        {
        case TypeList:
            "\n";
            sayList(val, depth + 1);
            break;

        case TypeSString:
            " '<<rexReplace('[\'\\\n\t\b]', val, new function(m) {
                return ['\'' -> '\\\'', '\\' -> '\\\\',
                    '\n' -> '\\n', '\t' -> '\\t', '\b' -> '\\b',
                        * -> '\\?'][m];
                })>>'";
            break;

        case TypeInt:
            " <<val>> (0x<<toString(val, 16)>>)";
            break;

        case TypeObject:
            if (val.ofKind(BigNumber))
            {
                if (val.getScale() < -4 || val.getScale() > 6)
                    " <<val.formatString(-1, BignumExp | BignumExpSign)>>";
                else
                    " <<val>>";

                if (val.getWhole() == val && val.getScale() <= 20)
                    " (0x<<bignumToHex(val)>>)";
            }
            else if (val.ofKind(ByteArray))
            {
                " <<rexReplace('..', val.unpackBytes(1, 'H*')[1], '%* ')>>";
//                " <<val.mapToString(new CharacterSet('latin1'))>>";
            }
            else
                " (object)";
            break;

        default:
            " <<val>>";
            break;
        }
        "\n";
    }
    "<<tab(depth-1)>>]\n";
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
