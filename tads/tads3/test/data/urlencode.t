#include <tads.h>

main(args)
{
    local n = 1;

    local tests = [
        'this is a test string',
        'another one? yes--really! we\'ll see how it goes...',
        'under_score',
        '!@#$%^&*()+=[]{}\|;\':",./<>?',
        'A-grave \u00C0, A-acute \u00C1, i-umlaut \u00ef, L-stroke \u0141',
        'a-grave \u00E0, a-acute \u00E1, c-cedilla \u00E7, OE \u0152',
        'R-caron \u0158, r-caron \u0159, dje \u0452, 3/8 \u215C',
        'ldquo \u201c, rdquo \u201d'
    ];
    "Two-way tests:\b";
    for (local i = 1, local len = tests.length() ; i <= len ; ++i, ++n)
    {
        local o = tests[i];
        local e = o.urlEncode();
        local d = e.urlDecode();

        "<<n>>:
        \no = <<o.htmlify()>>
        \ne = <<e.htmlify()>>
        \nd = <<d.htmlify()>>
        \nd == o ? <<d == o ? 'yes' : 'NO!!'>>
        \b";
    }

    "Invalid character decoding tests:\b";
    local inv = [
        '%80%81%82%83%84%85',
        '%c3%a0%a0%c5%92%92'
    ];
    for (local i = 1, local len = inv.length() ; i <= len ; ++i, ++n)
    {
        local o = inv[i];
        local d = o.urlDecode();

        "<<n>>:
        \no = <<o.htmlify()>>
        \nd = <<d.htmlify()>>
        \b";
    }
}
