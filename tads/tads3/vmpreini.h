/* $Header$ */

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmpreini.h - preinit
Function
  
Notes
  
Modified
  07/21/99 MJRoberts  - Creation
*/

#ifndef VMPREINI_H
#define VMPREINI_H

/*
 *   Run preinitialization.  Loads the image file, invokes its main
 *   entrypoint, and saves the new file. 
 */
void vm_run_preinit(class CVmFile *origfp, const char *orig_image_fname,
                    class CVmFile *newfp, class CVmHostIfc *hostifc,
                    class CVmMainClientIfc *clientifc,
                    const char *const *argv, int argc,
                    class CVmRuntimeSymbols *global_symtab,
                    class CVmRuntimeSymbols *macros);

#endif /* VMPREINI_H */

