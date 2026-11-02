//
//  common_utils.h
//  Spatterlight
//
//  Created by Administrator on 2025-12-20.
//

#ifndef common_utils_h
#define common_utils_h

#include "glk.h"

GLK_ATTRIBUTE_NORETURN void Fatal(const char *x);
winid_t FindGlkWindowWithRock(glui32 rock);

#endif /* common_utils_h */
