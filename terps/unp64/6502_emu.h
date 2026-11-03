/* Code from Exomizer, as changed for UNP64.
 *
 * Copyright (c) 2002 - 2023 Magnus Lind
 * Modifications for UNP64: iAN CooG and Petter Sjölund
 * C++ version based on code adapted to ScummVM by Avijeet Maurya
 *
 * Distributed under the zlib License by kind permission of the original
 * authors Magnus Lind and iAN CooG. The notice is in the file LICENSE in
 * this directory (terps/unp64), which has to go wherever this file goes.
 */

#ifndef UNP64_6502_EMU_H
#define UNP64_6502_EMU_H

#include <cstdint>

namespace Unp64 {

struct CpuCtx {
	/* Counted only to give the raster register something to return; nothing
	 * is timed by it. */
	uint32_t _cycles = 0;
	uint16_t _pc = 0;
	uint8_t *_mem = nullptr;
	uint8_t _sp = 0;
	uint8_t _flags = 0;
	uint8_t _a = 0;
	uint8_t _x = 0;
	uint8_t _y = 0;

	/* What the I/O chips read back as, for as long as one unp64() call
	 * lasts (the layers of a nested packer see one continuous machine). */
	int _d011[2] = {0, 0}; /* VIC $D011 as last written, and the raster line in $D012 */
	int _retfire = 0xff;   /* CIA1 $DC00: joystick 2, fire toggles on every read */
	int _retspace = 0xff;  /* CIA1 $DC01: keyboard, space toggles on every read */
};

/* Run one instruction. True if the emulation has to stop: an opcode that is
 * not implemented, or BRK. */
bool nextInst(CpuCtx *r);

} // End of namespace Unp64

#endif
