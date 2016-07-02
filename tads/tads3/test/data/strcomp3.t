#charset "cp1252"
#include <tads.h>
#include <strcomp.h>

main(args)
{
    local comp = new StringComparator(6, nil,
        [
         ['\u00E7', 'ss', 0x0100, 0x0200],  // c-cedilla, Windows Alt+0231
         ['\u00E0', 'a',  0x1000, 0x2000],  // a-grave, Windows Alt+0224
         ['\u00E1', 'a',  0x4000, 0x8000]   // a-acute, Windows Alt+0225
        ]);

    local tests = [
        ['da�', 'da\u00DF', 0x0001],  // match
        ['da�', 'das',      0x0000],  // (no match)
        ['da�', 'dass',     0x0003],  // uc + match (case folding � to ss)
        ['da�', 'DA�',      0x0003],  // uc + match
        ['da�', 'DAS',      0x0000],  // (no match)
        ['da�', 'DASS',     0x0003],  // uc + match (folding SS *and* � to ss)

        ['da�', 'da�',      0x0001],  // match
        ['da�', 'das',      0x0000],  // (no match)
        ['da�', 'dass',     0x0201],  // c-cedilla/ss + match
        ['da�', 'DA�',      0x0003],  // uc + match
        ['da�', 'DAS',      0x0000],  // (no match)
        ['da�', 'DASS',     0x0103],  // uc + c-cedilla/ss + match

        ['h�t', 'h\u00E0t', 0x0001],  // match
        ['h�t', 'hat',      0x2001],  // lc a-grave + match
        ['h�t', 'het',      0x0000],  // no match
        ['h�t', 'H\u00E0T', 0x0003],  // uc + match
        ['h�t', 'HAT',      0x1003],  // uc a-grave + uc match
        ['h�t', 'HET',      0x0000],  // no match

        ['h�t', 'h\u00E1t', 0x0001],  // match
        ['h�t', 'hat',      0x8001],  // lc a-acute + match
        ['h�t', 'het',      0x0000],  // no match
        ['h�t', 'H\u00E1T', 0x0003],  // uc + match
        ['h�t', 'HAT',      0x4003],  // uc a-acute + uc match
        ['h�t', 'HET',      0x0000]   // no match
    ];

    foreach (local t in tests)
    {
        local m = comp.matchValues(t[2], t[1]);
        local h1 = comp.calcHash(t[1]);
        local h2 = comp.calcHash(t[2]);
        "dict=<<t[1]>> (hash=<<h1>>), input=<<t[2]>> (hash=<<h2>>):
        match=<<toString(m, 16)>>, expected=<<toString(t[3], 16)>> - <<
          m == t[3] && (m == 0 || h1 == h2) ? "OK" :
          m == t[3] ? "*** Hash Error ***" : "*** Match Error ***">>\n";
    }
}

