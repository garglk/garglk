/*
 *   test of object definitions using braces 
 */

#include "tads.h"

object template "sdesc" 'name';

class Book: object ;

blueBook: Book
{
    name = 'blue book'
    sdesc = "blue book"
    ldesc = "It's a blue book."
}

redBook: Book
    name = 'red book'
    sdesc = "red book"
    ldesc = "It's a red book."
;

greenBook: Book "green book" 'green book'
    ldesc = "It's a green book."
;

yellowBook: Book "yellow book" 'yellow book'
{
    ldesc = "It's a yellow book."
}

orangeBook: Book
{
    "orange book" 'orange book'
    ldesc = "It's an orange book."
}

Book "purple book" 'purple book'
    ldesc = "It's a purple book."
;

Book
{
    "brown book" 'brown book';
    ldesc = "It's a brown book.";
}

main(args)
{
    forEachInstance(Book, { x: "sdesc = <<x.sdesc>>\n" });
}

