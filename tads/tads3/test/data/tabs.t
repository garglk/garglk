#include <tads.h>

main(args)
{
    "This is a test of some character-mode tabbing.\b";

    "----------<tab id=one>1-----<tab id=two>2---------------<tab id=three>3";
    "---<tab id=four>4-----------------<tab id=five>5---------";
    "--------<tab id=six>6------\n";
    "<tab to=one>one<tab to=three>three\n";
    "<tab to=two>two<tab to=four>four\n";
    "<tab to=one align=center>one-center<tab to=two>two
    <tab to=three align=right>three-right\n";
    "<tab to=two align=decimal dp='.'>dec.2
    <tab to=three align=right>three-right\n";

    "<tab align=center>Some text centered in the line\n
    <tab to=two>Two<tab align=center>Centered after that\n
    <tab align=right>Right-aligned text\n
    Text<tab to=two align=right>Two-right<tab align=right>Some
    right-aligned end text\n";
}

