>>header line 1<<
/* Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved. */
/*
Name
  header.t - header for inclusion from main.t
Function
  This is a test header for test_tok
Notes
  
Modified
  04/17/99 MJRoberts  - Creation
*/

'We\'re inside the header now!!!';

"here's an unterminated #if...";
#if 1

#include "header2.t"

"__LINE__ and __FILE__ in the header: " __FILE__, __LINE__;

"That's it for the header.";
>>header end<<
