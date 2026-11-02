/* Part of a cut-down version of UNP64, the generic Commodore 64 prg
 * unpacker, with only what is needed to decompress a number of Commodore 64
 * adventure games.
 *
 * (C) 2008-2022 iAN CooG/HVSC Crew^C64Intros
 * Copyright (c) 2002 - 2023 Magnus Lind
 * C++ version based on code adapted to ScummVM by Avijeet Maurya
 *
 * Distributed under the zlib License by kind permission of the original
 * authors Magnus Lind and iAN CooG. The notice is in the file LICENSE in
 * this directory (terps/unp64), which has to go wherever this file goes.
 */

#ifndef UNP64_H
#define UNP64_H

#include <cstddef>
#include <cstdint>

namespace Unp64 {

struct LoadInfo;

/* C64 addresses that the scanners need as well as the driver. */
constexpr int kZpVarTab = 0x2d;  /* start of BASIC variables = end of program */
constexpr int kBasicStart = 0x801;  /* where a BASIC program loads */
constexpr int kBasicRun = 0xa7ae; /* the interpreter loop: RUN the program */

/* The state of one unpacking pass: what the scanners found out about the
 * packer, what the switches asked for, and what the driver tracks while the
 * depacker runs.
 *
 * What a scanner may rely on, and has to keep to:
 *
 *  - It is called once per pass, with the program loaded into _mem,
 *    _info filled in (_info->_run is -1 if there is no SYS line) and every
 *    field below at its default: _depAdr 0, _retAdr and _strMem $0800,
 *    _endAdr $10000, the flags 0. The switches are applied afterwards and
 *    override what it sets.
 *  - _depAdr is the one field that has to be set for a packer that is
 *    recognised: the address of the depacker proper, once the stub has
 *    moved it into place. The driver emulates from the entry point until
 *    the program counter gets there, notes the pointers (_fEndBf, _fStrBf),
 *    and goes on until it reaches _retAdr or leaves for BASIC or the KERNAL.
 *  - Every block in a scanner is guarded by "_depAdr == 0", meaning that
 *    nothing has matched so far. A block that sets _depAdr without setting
 *    _idFlag lets the scanners after it run, and they skip their own blocks.
 *  - _idFlag = 1 stops the scanning. Set it when the match is certain.
 *  - _endAdr and _strMem are either an address, or (below $100) the zero
 *    page location that holds it when the depacker is done.
 *  - A scanner may patch _mem: skip a JSR into the KERNAL, rewrite the SYS
 *    line for the next layer. It may set _info->_run, and _recurs to have
 *    the output run through the scanners again.
 *  - Do not test _idFlag for anything else than that, and do not use it as
 *    scratch. */
struct UnpStr {
	int _idFlag;          /* flag, 1=packer identified: stop scanning */
	int _forced;          /* forced entry point */
	int _strMem;          /* start of unpacked memory */
	int _retAdr;          /* return address after unpacking */
	int _depAdr;          /* unpacker entry point */
	int _endAdr;          /* end of unpacked memory */
	int _rtAFrc;          /* flag, return address must be exactly RetAdr, else anything >= RetAdr */
	int _wrMemF;          /* flag, clean unwritten memory */
	int _lfMemF;          /* flag, clean end memory leftovers */
	int _exoFnd;          /* flag, Exomizer detected */ 
	int _fStack;          /* flag, fill stack with 0 and SP=$ff, else as in C64 */
	int _ecaFlg;          /* ECA found, holds relocated areas high bytes */
	int _fEndBf;          /* End memory address pointer before unpacking, set when DepAdr is reached */
	int _fEndAf;          /* End memory address pointer after  unpacking, set when RetAdr is reached */
	int _fStrBf;          /* Start memory address pointer before unpacking, set when DepAdr is reached */
	int _fStrAf;          /* Start memory address pointer after  unpacking, set when RetAdr is reached */
	int _idOnly;          /* flag, just identify packer and exit */
	int _rtiFrc;          /* flag, RTI instruction forces return from unpacker */
	int _recurs;          /* recursion counter */
	int _keypressIdx;     /* index into the canned GETIN keypress ring */
	unsigned int _monEnd;	/* End memory address pointers monitored during execution, updated every time DepAdr is reached */
	unsigned int _monStr;	/* Start memory address pointers monitored during execution, updated every time DepAdr is reached */
	unsigned int _mon1st;	/* flag for forcingly assign monitored str/end ptr the 1st time */
	unsigned int _endAdC;	/* add fixed values and/or registers AXY to End memory address */
	unsigned int _strAdC;	/* add fixed values and/or registers AXY to Start memory address */
	unsigned int _filler;	/* Memory filler byte*/
	unsigned char *_mem;    /* pointer to the memory array */
	LoadInfo *_info; /* pointer to the loaded prg info struct */
};

#define EA_USE_A 0x01000000
#define EA_USE_X 0x00100000
#define EA_USE_Y 0x00010000
#define EA_ADDFF 0x10000000
#define ITERMAX 0x02000000
#define RECUMAX 16

void scanners(UnpStr *);

/* Unpack a crunched C64 program. This is unp64() of unp64_interface.h, which
 * see, with one thing more:
 *
 *   destBufferCapacity   size in bytes of destinationBuffer; the unpacker
 *                        fails rather than overrun it (the biggest possible
 *                        output is 65536 bytes)
 */
bool unp64cpp(const uint8_t *compressed, size_t length, uint8_t *destinationBuffer, size_t destBufferCapacity, size_t *finalLength, const char *switches);

} // End of namespace Unp64

#endif
