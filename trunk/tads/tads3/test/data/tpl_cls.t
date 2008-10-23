#include <tads.h>

class Box: Item
    sdesc = "box: <<inherited.sdesc>>"
;

class Book: Item
    sdesc = "bookName = <<bookName>>, itemName = <<itemName>>\n"
    bookName = '(bookName undefined)'
;

class Watch: Item
    sdesc = "watchName = <<watchName>>, itemName = <<itemName>>\n"
    watchName = '(watchName undefined)'
;

class Stopwatch: Watch
    sdesc = "stopwatch: <<inherited.sdesc>>"
;

class Item: object
    sdesc = "itemName = <<itemName>>\n"
    itemName = '(itemName undefined)'
;

class Thing: object
    name = '(name undefined)'
;

