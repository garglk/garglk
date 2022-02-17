//
//  load_TI99_4a.h
//  scott
//
//  Created by Administrator on 2022-02-12.
//

#ifndef load_TI99_4a_h
#define load_TI99_4a_h

#include <stdio.h>
#include <stdint.h>
#include "definitions.h"

GameIDType DetectTI994A(uint8_t **sf, size_t *extent);

#endif /* load_TI99_4a_h */
