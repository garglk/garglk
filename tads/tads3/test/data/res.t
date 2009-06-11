#include "tads.h"
#include "t3.h"

show_list(lst)
{
    "[";
    for (local i = 1 ; i <= lst.length() ; ++i)
    {
        if (i > 1)
            " ";
        tadsSay(lst[i]);
    }
    "]";
}

main(args)
{
    "<center><font size=+2 color=blue><i>Resource Test</i></font></center>
    <br><br><br>
    <b>Welcome to the resource test!</b>  The point of this test is to
    try showing some images that are stored as embedded resources.  So,
    here are a couple of pictures...
    <br>
    <center>
    <img src='test/larry.jpg'>
    <br>
    <img src='test/bill.jpg'>
    <br>
    <img src='test/about3.jpg'>
    </center>
    <br>
    That's about it!
    <br><br>";
}

