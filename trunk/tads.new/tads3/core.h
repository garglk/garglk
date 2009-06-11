/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  core.h - sample T3 source code defining an intrinsic function set
  for the 'vmcore.cpp' sample
Function
  
Notes
  
Modified
  04/06/02 MJRoberts  - Creation
*/

#ifndef CORE_H
#define CORE_H

intrinsic 'core-sample/010000'
{
    /* display the given string */
    displayText(str);
    
    /* read a line of text from the keyboard, and return it as a string */
    readText();
}

#endif /* CORE_H */
