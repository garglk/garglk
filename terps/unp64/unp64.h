/* This is a cut-down version of UNP64 with only the bare minimum
 * needed to decompress a number of Commodore 64 adventure games.
 * It is distributed under the zlib License by kind permission of
 * the original authors Magnus Lind and iAN CooG.
 */

/*
 UNP64 - generic Commodore 64 prg unpacker
 (C) 2008-2022 iAN CooG/HVSC Crew^C64Intros
 original source and idea: testrun.c, taken from exo20b7

 Follows original disclaimer
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
 C++ version based on code adapted to ScummVM by Avijeet Maurya
 */

#ifndef UNP64_H
#define UNP64_H

namespace Unp64 {

struct LoadInfo;
struct CpuCtx;

struct UnpStr {
	int _idFlag;          /* flag, 1=packer identified; 2=not a packer, stop scanning */
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
	int _debugP;          /* flag, verbosely emit various infos */
	int _rtiFrc;          /* flag, RTI instruction forces return from unpacker */
	int _recurs;          /* recursion counter */
	unsigned int _monEnd;	/* End memory address pointers monitored during execution, updated every time DepAdr is reached */
	unsigned int _monStr;	/* Start memory address pointers monitored during execution, updated every time DepAdr is reached */
	unsigned int _mon1st;	/* flag for forcingly assign monitored str/end ptr the 1st time */
	unsigned int _endAdC;	/* add fixed values and/or registers AXY to End memory address */
	unsigned int _strAdC;	/* add fixed values and/or registers AXY to Start memory address */
	unsigned int _filler;	/* Memory filler byte*/
	unsigned char *_mem;    /* pointer to the memory array */
	char *_name;            /* name of the prg file */
	LoadInfo *_info; /* pointer to the loaded prg info struct */
	CpuCtx *_r;      /* pointer to the registers struct */
};

typedef void (*Scnptr)(UnpStr *);

#define EA_USE_A 0x01000000
#define EA_USE_X 0x00100000
#define EA_USE_Y 0x00010000
#define EA_ADDFF 0x10000000
#define ITERMAX 0x02000000
#define RECUMAX 16

void scanners(UnpStr *);

} // End of namespace Unp64

#endif
