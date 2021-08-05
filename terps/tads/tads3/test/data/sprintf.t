#include <tads.h>
#include <bignum.h>

main(args)
{
    test('Hello!');
    test('Percent %% test!');

    test('Integers: decimal=%d, unsigned=%u, octal=%o, hex=%x, '
         + 'HEX=%X, binary=%b',
         -123, -456, 03344, 0x55aa66bb, 0x88cc99dd, 0xf0f0);
    test('Unsigned: u=%u, u=%u, hex=%x, hex=%x, octal=%o, octal=%o',
         -12345, -12345.0, -255, -255.0, -1023, -1023.0);
    test('Plus signs: %+d, %+d, %+d, %+d', 123, -456, 7890, 0);
    test('Plus w/space: % d, % d, % d, % d', 123, -456, 7890, 0);
    test('# format: %#x, %#x, %#X, %#X, %#o, %#o',
         0, 0x123abc, 0, 0x345def, 0, 07654);

    test('Roman numerals I: %r, %R, %r, %R, %r, %R',
         0, -5, 100, 111, 3888, 4999);
    test('Roman numerals II: %r, %R, %r, %R, %r, %R',
         true, nil, 123.45, 678.90, ' 517!!', 9999);

    test('Integers with field widths: [%8d] [%_ 8d] [%_*8d] [%016d] [%_*8x]',
         100, 200, -1234, 4567, 0x400);
    test('Field widths too small: [%4d] [%4d] [%4d] [%4x]',
         1234, -123, 12345, 0xabcde);
    test('Left align: [%-5d] [%-5d] [%-5d] [%-5d] [%_*-10d] [%- 5d]',
         123, -1234, 12345, 123456, 87654, 987);

    test('Integers with precisions: [%+.8d] [%.20d] [%.8x] [%.8o]',
         123, -1234567890987.0, 0x456abc, 0334455);

    test('Grouping: [%,d] [%,d] [%,d] [%,d] [%,8d] [%,10d] [%,10d]',
         1, 12, 123, 1234, 1234567, 1234567, -1234567);

    test('Positionals: %[3]d, %[2]d, %d, %d, %[1]d, %d', 1, 2, 3);
    test('Positionals with widths: [%[2]6d], [%[1]06d]', 100, 200);

    test('Strings as integers: %d, %x, %d', '12345', '65535', '987.654');
    test('BigNumbers as integers: %d, %d, %d, %d, %d, %020x',
         0.5, 0.5001, 1.5, 1.75, 9111222333.0, 2.0.raiseToPower(68)-1);

    test('Strings: [%s] [%s] [%s] [%s] [%s]',
         'This is a test string!', 100, 123.4567, nil);
    test('Strings with widths: [%8s] [%8s] [%8s]',
         'Short', 'Too long for the limit', 'Right on');
    test('Strings w/widths, left-aligned: [%-8s] [%-8s] [%-8s]',
         'Short', 'Too long for the limit', 'Right on');
    test('Strings with limits: [%8.8s] [%8.8s] [%8.8s]',
         'Short', 'Too long for the limit', 'Perfect!');

    test('Chars: [%c] [%c] [%c] [%c]', 65, 'hello', 97.1, nil);

    test('f format: %f, %f, %f, %f',
         1234, 321.123456789, 3456789, 3.456e-20);
    test('f with precision: %.10f, %.20f, %.3f',
         1234.567890987654321, 8899.001122334455667788998877, 789.87654321);
    test('f with zero precision: %.0f, %.0f, %.f',
         1234.567890987654321, 8899.001122334455667788998877, 789.87654321);
    test('f with zero precision and #: %#.0f, %#.0f, %#.f',
         1234.567890987654321, 8899.001122334455667788998877, 789.87654321);
    test('e format: %e, %e, %e, %e',
         123, -999444777, 1234567.890, 1000000.0);
    test('E format: %E, %E, %E',
         0.00456, 1.87654321e20, -3.14159265e-4567);
    test('e with precision: %.10e, %.3e, %.6e',
         0.012345, 0.7766554433, 777.888999111);
    test('e with zero precision: %.0e, %.e, %.e',
         0.012345, 0.7766554433, 777.888999111);
    test('e with zero precision and #: %#.0e, %#.e, %#.e',
         0.012345, 0.7766554433, 777.888999111);
    test('Floats with signs: %+f, %+f, %+f', 1234.5678, -9876.54321, 0);
    test('Floats with spaces: % f, % f, % f', 1234.5678, -9876.54321, 0);
    test('Floats with widths: [%10f] [%10f]', 10203040, .24681357);
    test('g format: %g, %g, %g, %g, %g',
         100, 123456789, -23456789, .0001234567, .00012345678);
    test('G format: %G, %G, %G',
         0.00012345, 0.000012345, 0.0000012345);
    test('More g: %g, %g, %g, %g',
         1234, 12345, 123456, 1234567);
    test('g with precision: %.3g, %.8g, %.5g',
         12.789, 123.456, 123456.789);
    test('#g: %#g, %#g, %#g, %#g, %#g, %#g',
         1234, 12345, 123456, 1234567, 123000, 123e20);
}

test(fmt, [args])
{
    "<<fmt.htmlify()>>\n-&gt; <<
      sprintf(fmt, args...).findReplace(' ', '\ ').htmlify()>>\b";
}
