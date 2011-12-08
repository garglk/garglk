#include <tads.h>

class Thing: object
    vocab = nil
    name = nil
    desc = nil
;

Thing template 'vocab' 'name'?;
Thing template 'vocab' 'name' 'desc'?;

book: Thing 'book' '''It's a dusty old tome. ''';

main(args)
{
    "book: vocab=<<book.vocab>>, name=<<book.name>>, desc=<<book.desc>>\n";
}

