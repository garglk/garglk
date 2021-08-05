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
        ['daß', 'da\u00DF', 0x0001],  // match
        ['daß', 'das',      0x0000],  // (no match)
        ['daß', 'dass',     0x0003],  // uc + match (case folding ß to ss)
        ['daß', 'DAß',      0x0003],  // uc + match
        ['daß', 'DAS',      0x0000],  // (no match)
        ['daß', 'DASS',     0x0003],  // uc + match (folding SS *and* ß to ss)

        ['daç', 'daç',      0x0001],  // match
        ['daç', 'das',      0x0000],  // (no match)
        ['daç', 'dass',     0x0201],  // c-cedilla/ss + match
        ['daç', 'DAç',      0x0003],  // uc + match
        ['daç', 'DAS',      0x0000],  // (no match)
        ['daç', 'DASS',     0x0103],  // uc + c-cedilla/ss + match

        ['hàt', 'h\u00E0t', 0x0001],  // match
        ['hàt', 'hat',      0x2001],  // lc a-grave + match
        ['hàt', 'het',      0x0000],  // no match
        ['hàt', 'H\u00E0T', 0x0003],  // uc + match
        ['hàt', 'HAT',      0x1003],  // uc a-grave + uc match
        ['hàt', 'HET',      0x0000],  // no match

        ['hát', 'h\u00E1t', 0x0001],  // match
        ['hát', 'hat',      0x8001],  // lc a-acute + match
        ['hát', 'het',      0x0000],  // no match
        ['hát', 'H\u00E1T', 0x0003],  // uc + match
        ['hát', 'HAT',      0x4003],  // uc a-acute + uc match
        ['hát', 'HET',      0x0000]   // no match
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

