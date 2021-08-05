#charset "latin1"

/*
 *   File.packBytes tests 
 */

#include <tads.h>
#include <file.h>
#include <bignum.h>
#include <bytearr.h>
#include <strbuf.h>

main(args)
{
    local fname = 'packfile.bin';
    writeTest(fname);
    readTest(fname);
}

writeTest(fname)
{    
    local fp = File.openRawFile(fname, FileAccessWrite);

    twriteSpecial(fp, 'a020', 'a10xa10', 'abcdefghij', 'lmnopqrstu');
    
    twrite(fp, '"this is a ""test"" of a literal string!"');
    twrite(fp, '"literal"5 "another"5');
    twrite(fp, '{404142434445}');
    twrite(fp, '{415A}10 {4D}5');

    twrite(fp, 'a0', 'null-terminated latin-1 string');
    twrite(fp, 'u0', 'null-terminated UTF-8 string: àáâãä');
    twrite(fp, 'w0', 'null-terminated wide character string: àáâãä');
    twrite(fp, 'h0', '1424344454');
    twrite(fp, 'H0', '4142434445');

    twrite(fp, 'a035', 'null-term A w/len');
    twrite(fp, 'u035', 'null-term U w/len: àáâãä');
    twrite(fp, 'w035', 'null-term W w/len: àáâãä');

    twrite(fp, 'a010', 'null-term trunc A');
    twrite(fp, 'u010', 'null-term trunc U');
    twrite(fp, 'w010', 'null-term trunc W');

    twrite(fp, '[l s]3', [1, 2, 3, 4, 5]);
    twrite(fp, 'C/[l s]', [1, 2, 3, 4, 5]);
    twrite(fp, 'C/[l s]', [1, 2, 3, 4, 5, 6]);
    twrite(fp, 'a4/s', [10, 11, 12, 13]);

    twrite(fp, 'h8!', '112233445566778899aa');
    twrite(fp, 'h8', '112233445566778899aa');
    twrite(fp, 'C/h!', '112233445566778899aa');
    twrite(fp, 'C/w!', 'UCS-2 with byte length prefix.');

    twrite(fp, 'a50', 'This is a nul-padded string with length 50.');
    twrite(fp, 'a20', 'a20 string, which should be truncated.');
    twrite(fp, 'A50', 'This is a space-padded string with length 50.');
    twrite(fp, 'u20', 'u20: Þßàáâãäæçèéêëìíîïðñòóôõö');
    twrite(fp, 'U20', 'U20: Þßàáâãäæçèéêëìíîïðñòóôõö');

    twrite(fp, 'w80 W80',
           'Here\'s a wide character string.',
           'Wide character with space padding.');

    twrite(fp, 'C/a', 'This is a test string with a length prefix.');
    twrite(fp, 'C/u', 'UTF-8 with length prefix: òóôõö.');
    twrite(fp, 'C/w', 'UCS-2 with length prefix: òóôõö.');
    twrite(fp, 'C/H', '4865782077697468206C656E67746820707265666978');
    twrite(fp, 'C/h C/H', '8656870282c69202f64646D',
           '68657820284229206F6464E');

    twrite(fp, 'a4/a', 'alpha length prefix');
    twrite(fp, 'C/[C/a]', ['group with length prefix',
                           'with strings with length prefixes!']);

    twrite(fp, '(C/a)3', 'group string one', 'second group string',
           'group string the third');

    twrite(fp, 'C/[[l C/[a6] s]]',
           [[0x12345678, ['one', 'two', 'three'], 0x2468],
            [0x55667788, ['four', 'five'], 0x3344],
            [0xdefcab01, ['six', 'seven', 'eight', 'nine'], 0x4321]]);

    twrite(fp, 'h20 H20', '8656870282c692', '68657820284229');

    twrite(fp, 'c3C5', [1, 127, -1, -128], [127, 128, 255]);

    twrite(fp, '[s3][S3]s>S>',
           [0x1234, 0x2345, 0x3456], [0x89ab, 0xabcd, 0xcdef],
           0x5566, 0xaabb);

    twrite(fp, 'l3L3(lL)>', [0x10203040, 0x12345678, 0x708090a0],
           [0x1234abcd, 0x11223344, 0x55667788],
           0x02040608, 0x0a0b0c0d);

    twrite(fp, 'q3[Q3]q>Q>', [0x7fffffff, -5000, 2.0.raiseToPower(63)-1],
           [2.0.raiseToPower(32)-1, 2.0.raiseToPower(64)-1, 12345678909876.0],
           0x10203040, 0x70a0b0c0);

    twrite(fp, 'L3~', 100.0, 2000000000.0, 4000000000.0);
    twrite(fp, 'L3>~', 100.0, 2000000000.0, 4000000000.0);
    twrite(fp, 'q4~', 100.0, 2000000000.0, 4000000000.0, 8000000000.0);
    twrite(fp, 'q4>~', 100.0, 2000000000.0, 4000000000.0, 8000000000.0);

    twrite(fp, '[f2][d2] f> d>', [1.0, 2], [-1.0, -2], 1.0, -2.0);
    twrite(fp, '[f2][d2](fd)>', [1234.5678, 999.999], [-888.888, -777.777],
           1000.0001, -2000.0002);

    twrite(fp, 'C/a@20a10@25a3@30x5a3', '***at sign***', '@20+-|-+-+', '@25',
           'past nuls!');
    twrite(fp, '(a10 @5 a5)3',
           'aaaaaaaaaa', 'AAAAA', 'bbbbbbbbbb', 'BBBBB',
           'cccccccccc', 'CCCCC');
    twrite(fp, 'a10 (a5 @5! a5)', 'aaaaaaaaaa', 'bbbbb', 'ccccc');

    twrite(fp, 'a x:s! s x:l! l a10 X:l! l',
           'A', 0x1234, 0x56789abc, 'ABCDEFGHIJKLMNOP', 0x11223344);

    twrite(fp, 'l X:l l X:l l', 0x11223344, 0x55667788, 0x99aabbcc);

    twrite(fp, 'C/as5', 'int array shorter than count', [1024, 1025]);

    local arr = new ByteArray(50);
    for (local i = 1 ; i <= 50 ; ++i)
        arr[i] = i + 102;

    twrite(fp, 'C/a C/a', 'byte array as char', arr);
    twrite(fp, 'C/a C/u', 'byte array as utf8', arr);
    twrite(fp, 'C/a C/b', 'byte array as bytes', arr);

    twrite(fp, 'l s a4/(s s)', 10, 11, 1, 2, 3, 4, 5);

    twrite(fp, 'k*', 1, 2, 127, 128, 129, 128*128+127, 4000000000.,
           2.0.raiseToPower(64)-1, 1234567890987654321.);

    fp.closeFile();
}

twrite(fp, fmt, [args])
{
    fp.packBytes('C/a', '{{{<<fmt>>}}}');
    fp.packBytes(fmt, args...);
}

twriteSpecial(fp, unpackFmt, packFmt, [args])
{
    fp.packBytes('C/a', '{{{<<unpackFmt>>}}}');
    fp.packBytes(packFmt, args...);
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

    fp.closeFile();
}

tread(fp)
{
    local fmt = fp.unpackBytes('C/a')[1];
    rexMatch('%{%{%{(.*)%}%}%}', fmt);
    fmt = rexGroup(1)[3];
    "Format code = <<fmt>> ";

    local lst = fp.unpackBytes(fmt);
    sayList(lst);
    "\n";
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
                " <<val>> (bignum)";
                if (val.getWhole() == val && val.getScale() <= 20)
                    " (0x<<bignumToHex(val)>>)";
            }
            else if (val.ofKind(ByteArray))
            {
                " <<val.mapToString(new CharacterSet('latin1'))>>";
            }
            else
                " (object)";
            break;

        default:
            " <<val>>";
            break;
        }
    }
    "]";
}

bignumToHex(val)
{
#if 1
    return toString(val, 16);
#else
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
#endif
}
