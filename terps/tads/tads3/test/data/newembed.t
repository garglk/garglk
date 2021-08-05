#include <tads.h>


class Thing: object
    desc()
    {
        ldesc;
        inherited();
    }
    ldesc = ""
;

enum small, medium, large, gigantic;

class Sizable: object
    desc()
    {
        "It appears to be <<if size == small>>small<<else if
           size == medium>>medium<<else if size == large>>large<<
           else if size == gigantic>>gigantic<<end>> in size. ";
        inherited();
    }
    size = medium
;

class Openable: object
    desc()
    {
        "The <<name>> is <<if isOpen>>open<<else>>closed<<end>>. ";
        inherited();
    }
    isOpen = true
    size = medium
;

class Moistenable: object
    desc()
    {
        "<<if isMoist>>It appears to be a bit damp. ";
        inherited();
    }
    isMoist = nil
;

box: Thing, Sizable, Openable, Moistenable
    name = 'box'
    ldesc = "It's a brown cardboard box. "
    isOpen = true
    size = large
    isMoist = true
;

cabinet: Thing, Sizable, Openable, Moistenable
    name = 'cabinet'
    ldesc = "It's a large oak cabinet. "
    isOpen = nil
;

oven: Thing
    name = 'oven'
    desc = "The oven door is <<if isOpen>>open<<if isLit>> and
           the light inside is on<<end>><<else>>closed<<end>>. "

    isOpen = nil
    isLit = nil

    test(o, l)
    {
        isOpen = o;
        isLit = l;
        desc;
        "\n";
    }
;

main(args)
{
    box.desc(); "\b";
    cabinet.desc(); "\b";

    for (local i in 1..10)
        "\*Nested if with i=<<i>>:
        <<if i < 5>><<if i < 3>>i is less than 3<<else
        >>i is at least 3 but less than 5<<else if i <= 7>>
        i is 5-7<<else>>i is greater than 7<<end>>\n";

    "\b";
    for (local i in 10..100 step 10)
    {
        local s = 'i=<<i>>, which is <<if i < 50>>less than
        fifty<<else if i < 70>>at least fifty, but less
        than seventy<<else>>seventy and up';
        "<<s>>\n";
    }

    "\b";
    oven.test(nil, nil);
    oven.test(nil, true);
    oven.test(true, nil);
    oven.test(true, true);

    "\b";
    local a = 5;
    """This is an "unless" test. <<unless a < 3>>'a' is not less than 3.
    <<end>><<unless a > 3>>'a' is not greater than 3.""";

    "\b";
    """Second "unless" test. <<unless a < 7>>'a' is not less than 7.""";

    "\b";
    """Third "unless" test. <<unless a < 3>>'a' is not less than 3.""";

    "\b";
    local s = '''Another "unless" test. <<unless a > 3>>'a' is not
    greater than 3.<<otherwise if a > 7>>'a' is more than 7.
    <<otherwise unless a > 6>>'a' is less than 6. ''';
    "<<s>>";

    // test from example in doc
    "\b";
    {
        local i = 3, j = 'boo';
        local s = 'The value of i is <<i>>, and j is <<j>>!';
        "s is: <<s>>\n";
        i = 7;
        j = 'boo hoo';
        "s is now: <<s>>\n";
    }

    // another doc example
    "\b";
    {
        local i = 3, j = 'boo';
        local s = {: 'The value of i is <<i>>, and j is <<j>>!'};
        "s is: <<s()>>\n";
        i = 7;
        j = 'boo hoo';
        "s is now: <<s()>>\n";
    }

    "\n";
}
