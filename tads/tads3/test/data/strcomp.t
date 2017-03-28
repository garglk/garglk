#charset "win1251"
#include <tads.h>
#include <strcomp.h>

main(args)
{
    local comp = new StringComparator(6, nil,
        [
         ['\u00DF', 'ss', 0x0100, 0x0200],  // Eszett, DOS Alt+225
         ['\u00E0', 'a',  0x1000, 0x2000],  // a-grave, DOS Alt+133
         ['\u00E1', 'a',  0x4000, 0x8000],  // a-acute, DOS Alt+160
         ['\u00FC', 'ue', 0x4000, 0x8000]   // u-umlaut, DOS Alt+129
        ]);

    "Type QUIT to quit.\n";
    for (;;)
    {
        local s1, s2;
        local m, h1, h2;

        
        "\bDictionary string: ";
        s1 = inputLine();
        if (s1.toLower() == 'quit')
            break;
        
        "Input string: ";
        s2 = inputLine();

        m = comp.matchValues(s2, s1);
        h1 = comp.calcHash(s1);
        h2 = comp.calcHash(s2);
        "Match = <<toString(m, 16)>>,
        hash(<<s1>>) = <<h1>>, hash(<<s2>>) = <<h2>>";

        if (m != 0 && h1 != h2)
            "\n*** HASH MISMATCH!!! ***\n";
    }
}

