#include <tads.h>
#include <strcomp.h>
#include <dict.h>

dictionary cmdDict;

main(args)
{
    "Hello, world!\n";
    cmdDict.addWord(testObj, 'test', &noun);
    cmdDict.addWord(initObj, 'init', &adjective);

    foreach (local w in ['washington', 'lee', 'gutierrez',
                         'pfister', 'jackson', 'tymczak',
                         'ashcraft', 'robert', 'rupert', 'rubin',
                         'test', 'init', 'taste', 'isn\'t', 'innuit', 'intuit',
                         'track', 'too'])
    {
        local m = cmdDict.findWord(w);
        "<<w>>: soundex=<<SoundexStringComparator.canonicalize(w)>>,
        <<m.length() != 0 ? 'found ' + m[1].name : 'not found'>>\n";
    }
}

testObj: object
    name = 'test'
;
initObj: object
    name = 'init'
;

test: InitObject
    execute() { 
        // This works when we use StringComparator 
        // instead of SoundexStringComparator here: 
        local cmpObj = 
            new SoundexStringComparator(0, nil, [['a','a',0,0]]); 
        cmdDict.setComparator(cmpObj);
        "...done with init...\n";
    } 
; 

class SoundexStringComparator: object 
   cmp = nil 
   construct(truncLen, caseSensitive, mappings)
   { 
       cmp = new StringComparator(truncLen, caseSensitive, mappings); 
   } 
   calcHash(str)
   {
       ". calcHash(<<str>>)...\n";
       return cmp.calcHash(canonicalize(str));
   } 
   matchValues(inputStr, dictStr)
   {
       ". matchValues(<<inputStr>>, <<dictStr>>)...\n";
       return cmp.matchValues(canonicalize(inputStr), canonicalize(dictStr)); 
   }
   soundexTab = ['b'->'1', 'f'->'1', 'p'->'1', 'v'->'1',
                 'c'->'2', 'g'->'2', 'j'->'2', 'k'->'2',
                 'q'->'2', 's'->'2', 'x'->'2', 'z'->'2',
                 'd'->'3', 't'->'3',
                 'l'->'4',
                 'm'->'5', 'n'->'5',
                 'r'->'6']
   canonicalize(str)
   {
       str = rexReplace('[bfpv][wh]+[bfpv]+'
                        + '|[cgjkqsxz][wh]+[cgjkqsxz]+'
                        + '|[dt][wh]+[dt]+'
                        + '|l[wh]+l+'
                        + '|[mn][wh]+[mn]+'
                        + '|r[wh]+r+', str,
                        {m, i, o: m.substr(1, 2)}, ReplaceAll);
       local first = str.substr(1, 1);
       str = rexReplace('[bfpvcgjkqsxzdtlmnr]', str,
                        {m, i, o: SoundexStringComparator.soundexTab[m]},
                        ReplaceAll);
       str = rexReplace('(<digit>)%1+', str, '%1', ReplaceAll);
       str = str.splice(1, 1, first);
       str = rexReplace('<^digit>', str, '', ReplaceAll);
       str = first + str;
       if (str.length() < 4)
           str += '000'.substr(str.length());
       else if (str.length() > 4)
           str = str.substr(1, 4);

       return str.toUpper();
   } 
;

