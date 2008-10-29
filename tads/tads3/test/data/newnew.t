obj1: object
    noun = 'book'
    adjective = 'blue'
;

f(x)
{
    return x*2;
}

_main(args, argc)
{
    local o;
    local l;

    o = new obj1;
//    o = new (obj1(3));
//    o = new (obj1(3))(5);
//    o = new (f(3))(5, 6);

    o = new Dictionary();
    o.addWord();
    l = o.findWord('book', &noun);
}
