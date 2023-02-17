/* Code from Exomizer distributed under the zlib License
 * by kind permission of the original authors
 * Magnus Lind and iAN CooG.
 */

/*
 * Copyright (c) 2002 - 2023 Magnus Lind.
 *
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 * claim that you wrote the original software. If you use this software
 * in a product, an acknowledgment in the product documentation would be
 * appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

/*
 Original: Magnus Lind
 Modifications for UNP64: iAN CooG and Petter Sjölund
 C++ version based on code adapted to ScummVM by Avijeet Maurya
 */


#ifndef UNP64_6502_EMU_H
#define UNP64_6502_EMU_H

namespace Unp64 {

struct CpuCtx {
	uint32_t _cycles;
	uint16_t _pc;
	uint8_t *_mem;
	uint8_t _sp;
	uint8_t _flags;
	uint8_t _a;
	uint8_t _x;
	uint8_t _y;
};

int nextInst(CpuCtx *r);

} // End of namespace Unp64

#endif
