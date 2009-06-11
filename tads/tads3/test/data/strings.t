/* Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved. */
/*
Name
  strings.t - test of multi-line strings for tokenizer
Function
  
Notes
  
Modified
  04/26/99 MJRoberts  - Creation
*/

"Here's a string on just one line.";

"This string is on
     two separate lines.";

"This string goes on
     for several lines,

     including a blank line

     or two


     or three!!!";

"This string runs on for three lines,
     and has some more material on the
     line where it ends.";   More_Material();

"This string has an /* embedded comment */
     and then goes on to a second line.";

Initial_Material(); "This string
    starts after some material, and
    has more after
    it ends.";  Final_Material();

"This string has an <<embedded_expression()>> on one line.";

"Here's an <<embedded_expression()>>
    on two lines.";

"An <<embedded_expression()
     >> with the expression split over two lines. ";

"More than just <<one_expression()>>, but
     even a <<second_expression()>>,
     and then a <<third_expression()
                 + even_more()>> on another line!";

"Here's a string that we'll
allow to appear unterminated.
;

"That's it!";
