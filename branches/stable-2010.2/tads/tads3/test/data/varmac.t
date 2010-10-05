#include <tads.h>

#define printf(prefix, arg...) \
   tadsSay(prefix); "\n"; \
   arg#foreach*"\t"; tadsSay(#@arg + ' = ' + arg); "\n"; **

#define makelist(ele...) [ele#foreach#ele#,#]

#define makelist2(nm, eles...) (nm = [eles#argcount, ## eles])

#define inheritNext(prop, args...) \
    doInheritNext(nil, &prop, args#foreach#args#,#)

#define printConcat(prefix, args...) \
    tadsSay(prefix args#ifnempty#,# args#foreach#args#+#)

main(args)
{
    local a = 1;
    local b = 2;
    local xyz = 3;
    local lst;
    
    printf('hello', a, b, xyz);
    printf('goodbye');

    "\blist 1 a:\n";
    lst = makelist(1, 2, 3);
    foreach(local cur in lst)
        "cur = <<cur>>\n";

    "\blist 1 b:\n";
    lst = makelist();
    foreach(local cur in lst)
        "cur = <<cur>>\n";

    "\blist 1 c:\n";
    lst = makelist(8);
    foreach(local cur in lst)
        "cur = <<cur>>\n";

    "\blist2 a:\n";
    makelist2(lst, 7, 8, 9, 10);
    foreach(local cur in lst)
        "cur = <<cur>>\n";

    "\blist2 b:\n";
    makelist2(lst);
    foreach(local cur in lst)
        "cur = <<cur>>\n";

    inheritNext(sdesc, 1, 2, 3);
    inheritNext(sdesc, 'hello');
    inheritNext(ldesc);

    "\b";
    printConcat('no-arguments'); "\n";
    printConcat('args', 'a', 'b', 'c'); "\n";
}

property sdesc, ldesc;

doInheritNext(obj, prop, [args])
{
    "doInheritNext: ";
    switch(prop)
    {
    case &sdesc: "sdesc"; break;
    case &ldesc: "ldesc"; break;
    default: "unknown prop"; break;
    }

    "\n";
    foreach(local cur in args)
        "\t<<cur>>\n";
}
