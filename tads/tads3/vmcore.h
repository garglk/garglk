/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmcore.h - T3 VM "core" interpreter - example function set definition
Function
  This is an example of how to define custom intrinsic (i.e., native C++)
  functions for the T3 VM.  An intrinsic function is a function written in
  C++, and part of the application executable, that can be called from TADS
  code (i.e., from within .t source).

  This file can be used as a template for creating custom intrinsics.
  Simply make a copy of this entire file, and then make the following
  changes:

  1. Change the #ifndef symbol from VMCORE_H to a different name of your
  own choosing - usually, the name matches the name of the .h file itself.
  This symbol appears three times in the file, so you should be careful to
  change all three to the same new symbol.

  2. Change the name of class CVmBifSample to a name of your own choosing.
  Choose a name that describes the custom function set you're defining.  This
  name appears in the class definition and also in each entry in the list
  near the bottom of the file, so make sure to change every occurrence of the
  name to the same new name.

  3. Change the names of the functions themselves.  Note that each function
  name appears twice - first in the class definition near the start of the
  file, then again in the initialization list at the bottom of the file.  You
  can add and delete functions as you wish.  If you add new functions, simply
  copy an existing definition and change its name, being sure to add the
  function in both the class definition and the list at the bottom of the
  file.

  4. You must, of course, provide an implementation for each of your
  functions.  Do that in a new .cpp file corresponding to your new header
  file.  Make sure you #include your new header file from your new .cpp file.
  You can use the implementations in vmcore.cpp as a template for your new
  functions.

  5. Make a copy of the file vmbifreg_core.cpp.  Add a #include line for
  the new copy of this file you have created (you can add it right after the
  #include for vmbiftad.h).  Also add a MAKE_ENTRY() line for your new
  function set - you can add it where the comments indicate in your copy of
  vmbifreg_core.cpp.  The MAKE_ENTRY() line would look like this:

    MAKE_ENTRY("core-sample/010000", CVmBifSample);

  Use the same name you use for the variable defined in the initialization
  list near the end of the file.  For the string, you can use anything you
  want, but you should choose something reasonably long and descriptive to
  avoid overlapping with anyone else's function set identifiers.

  6. Take vmbifreg_core.obj out of the application build, and replace it
  with the copy you created in step 5.

  7. Take vmcore.obj out of the application build, and replace it with your
  new .cpp file from step 4.

  8. Change the variable name G_bif_sample near the end of this file to a
  different name of your own choosing.

  9. Create a TADS header file, for inclusion in your TADS (.t) source
  code, defining your intrinsic functions.  See the sample file "core.h"

    intrinsic 'core-sample/010000'
    {
      // display the given string
      displayText(str);

      // read a line of text from the keyboard, and return it as a string
      readText();
    }

  You MUST define the intrinsic functions in the EXACT SAME ORDER that they
  appear in the initialization list near the end of this file.  For the name
  of the function set (the 'core-sample/010000' string), you must use the
  EXACT SAME string you used in the MAKE_ENTRY() definition in step 5.

  Everything else in this file is boilerplate text, so you shouldn't have
  to change anything else.

Notes
  
Modified
  04/06/02 MJRoberts  - Creation
*/

#ifndef VMCORE_H
#define VMCORE_H

#include "vmbif.h"

/* ------------------------------------------------------------------------ */
/*
 *   Our sample built-in functions.  Each function we define here will be
 *   callable as an intrinsic function from TADS source code.
 *   
 *   This is only the interface definition.  The actual implementations of
 *   these functions appear in our .cpp file.
 *   
 *   Note that every function has the same C++ interface.  This has nothing
 *   to do with the arguments that the TADS program passes to these
 *   functions; the functions get their TADS arguments from the VM stack.
 *   See the implementations for details.  
 */
class CVmBifSample: public CVmBif
{
public:
    /* 
     *   The function vector.  Every function-set class must have a static
     *   member called 'bif_table' containing an array of pointers to the
     *   functions in the set.  The registration mechanism uses this to
     *   connect calls from the image file to their actual C++
     *   implementations.  The order of the vector must match the order
     *   defined in the library header file for the function set, since the
     *   compiler generates ordinal references to the member functions.  
     */
    static vm_bif_desc bif_table[];

    /* function set member functions */
    static void display_text(VMG_ uint argc);
    static void read_text(VMG_ uint argc);
};

#endif /* VMCORE_H */

/* ------------------------------------------------------------------------ */
/*
 *   Sample function set vector.  Define this only if VMBIF_DEFINE_VECTOR has
 *   been defined.
 *   
 *   IMPORTANT - this definition is outside the #ifdef VMCORE_H section of
 *   the header file, because we specifically want this part of the file to
 *   be able to be included multiple times.
 *   
 *   ALSO IMPORTANT - the ORDER of the definitions here is significant.  You
 *   must use the EXACT SAME ORDER in your "intrinsic" definition in the
 *   header file you create for inclusion in your TADS (.t) source code.
 *   
 *   The vector must always be called 'bif_table', and it must be a static
 *   member of the function-set class.  The order of the functions defined
 *   here MUST match the order in the library header file for the function
 *   set, since the compiler generates ordinal references to the functions.  
 */
#ifdef VMBIF_DEFINE_VECTOR

vm_bif_desc CVmBifSample::bif_table[] =
{
    { &CVmBifSample::display_text, 1, 0, FALSE },
    { &CVmBifSample::read_text, 0, 0, FALSE }
};

#endif
