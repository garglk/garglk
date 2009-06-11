#include <tads.h>
#include <strcomp.h>
#include <dict.h>

dictionary cmdDict;

main(args)
{
    "Hello, world!\n";
    cmdDict.addWord(test, 'test', &noun);
    cmdDict.addWord(test, 'init', &adjective);

    local x = cmdDict.findWord('test');
}

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
   construct(truncLen, caseSensitive, mappings) { 
       cmp = 
           new StringComparator(truncLen, caseSensitive, mappings); 
   } 
   calcHash(str) { "...calcHash...\n"; return cmp.calcHash(canonicalize(str)); } 
   matchValues(inputStr, dictStr) {
        "...matchValues...\n";
     return cmp.matchValues(canonicalize(inputStr), 
                            canonicalize(dictStr)); 
   } 
   canonicalize(str) { 
       return str; //soundex(str); 
   } 
;

