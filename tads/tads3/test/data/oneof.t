#include <tads.h>

main(args)
{
    local s;
    local a = 123, b = 456, c = 789;
    
    for (local i in 1..50)
    {
        s = 'The random mood is <<one of>>happy<<or>>sad<<or>>morose<<
              or>>elated<<or>>excited<<or>>melancholy<<or>>thrilled<<
              or>>confused<<or>>curious<<or>>indifferent<<or>>middling<<
              then shuffled>>. ';
        "<<s>>\n";
    }

    "\b";
    for (local i in 1..50)
    {
        s = '''Let's pick a variable to show: <<one of>>a=<<a>><<
              or>>b=<<b>><<or>>c=<<c>><<then at random>>.''';
        "<<s>>\n";
    }

    "\b";
    for (local i in 1..50)
    {
        "Cycling through colors to <<one of>>red<<or>>blue<<or>>green<<
          or>>yellow<<or>>orange<<or>>purple<<or>>magenta<<or>>cyan<<
          or>>gold<<cycling>>.\n";
    }

    "\b";
    for (local i in 1..50)
    {
        "Randomly picking variables again: <<one of>>a = <<a>><<
          or>>b = <<b>><<or>>c = <<c>><<purely at random>>.\n";
    }

    "\b";
    for (local i in 1..50)
    {
        "Nested if &gt; oneof [<<i>>]: <<if i < 25>><<
          one of>>red<<or>>blue<<or>>green<<at random>><<else
          >><<one of>>yellow<<or>>orange<<or>>purple<<at random>><<
          end>>\n";
    }

    "\b";
    for (local i in 1..50)
    {
        "Nested oneof &gt; if [<<i>>]: <<one of
          >><<if i < 25>>red<<else>>yellow<<or
          >><<if i < 25>>blue<<else>>orange<<or
          >><<if i < 25>>green<<else>>purple<<at random>>\n";
    }

    "\b";
    for (local i in 1..5)
        "First time only [<<i>>] <<first time>>- here it is!<<only>>...\n";
}
