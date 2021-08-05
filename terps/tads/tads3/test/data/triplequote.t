/*
 *   comment!  
 */
#include <tads.h>

main(args)
{
    local i = 123, j = 345;

    """""Test of various mode switching. <<'''Triple-single'''>>.""";

    """More mode switching. <<'Single single', '''Triple single'''>> end.""";
    local s = '''Even more. <<'Single single', '''Triple single'''>>
    and back to it.''';
    "<<s>>\n";

    """Again with the doubles. <<"Double-single", """Double-triple""">>
    and ending.""";
    
    """This is a triple-quoted string.  "It's very nice," someone
    said.  What about ""pairs of quotes""?  They should be fine.
    And we should also handle a whole run of escaped quotes,
    as in these 10: \"""""""""".  That's it for this one.""";

    "\b";
    """"And how about quotes around the whole string?"""";

    "\b";
    """""Or ""pairs"" of quotes???""""";

    "\b";
    "There's the empty triple-quoted string, of course:";
    """""";

    "\b";
    "And what about a string of 16 quotes? As in: "; """""""""""""""";

    "\b";
    "This is a regular string with i=<<i>> embedded.";

    "\b";
    """This is a triple quoted string with "embedded quotes"!!!""";

    "\b";
    """On to triple quote embeddings. i=<<i>>, j=<<j>>, and
    is i==j? <<i == j ? '''Yes!''' : 'No.'>>  How about i!=j? <<
    i != j ? '''Yes!''' : 'No.'>>  And using if: <<if i==j>>i equals
    j<<else>>i and j are unequal<<end>>.""";
        
    "\n";
}
