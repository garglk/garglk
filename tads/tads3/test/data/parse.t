/*
 *   A simple parser for a few phrases
 */

#include "tads.h"
#include "t3.h"
#include "dict.h"
#include "gramprod.h"

grammar command: predicate->pred_ cmdTerminator *
    debugPrint(level)
        { indent(level); "command pred:\n"; pred_.debugPrint(level+1); }
;

grammar cmdTerminator: 'then' | '.' | '!' | ';' | '?'
    | 'and' 'then' | 'and'
    | ',' 'then' | ',' 'and' 'then' | ',' 'and' | ',' : object
;

grammar predicate: 'say' tokString->str_ : object
    debugPrint(level)
        { indent(level); "predicate tadsSay(str_ = <<str_>>)\n"; }
;

grammar predicate: 'take' nounPhrase->np_ : object
    debugPrint(level)
        { indent(level); "predicate take np:\n"; np_.debugPrint(level+1); }
;

grammar predicate: 'give' nounPhrase->dobj_ 'to' nounPhrase->iobj_
    | 'give' nounPhrase->iobj_ nounPhrase->dobj_ : object
    debugPrint(level)
    {
        indent(level);
        "predicate give dobj, iobj:\n";
        dobj_.debugPrint(level+1);
        iobj_.debugPrint(level+1);
    }
;

grammar nounPhrase: noun->noun_ : object
    debugPrint(level)
        { indent(level); "nounPhrase(noun = <<noun_>>)\n"; }
;

grammar nounPhrase: adjective->adj_ nounPhrase->np_ : object
    debugPrint(level)
    {
        indent(level);
        "nounPhrase(adj = <<adj_>>) np:\n";
        np_.debugPrint(level+1);
    }
;
