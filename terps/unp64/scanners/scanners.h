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
 * the directory above (terps/unp64), which has to go wherever this file goes.
 */

#ifndef UNP64_SCANNERS_H
#define UNP64_SCANNERS_H

#include "exo_util.h"
#include "unp64.h"

namespace Unp64 {

/* One scanner per packer family. Each looks at the loaded program in
 * unp->_mem for the depacker stubs it knows; the protocol they follow is
 * described above UnpStr in unp64.h. */
void scnAbuzeCrunch(UnpStr *unp);
void scnActionPacker(UnpStr *unp);
void scnAR(UnpStr *unp);
void scnByteBoiler(UnpStr *unp);
void scnCaution(UnpStr *unp);
void scnCCS(UnpStr *unp);
void scnCruel(UnpStr *unp);
void scnECA(UnpStr *unp);
void scnExpert(UnpStr *unp);
void scnExomizer(UnpStr *unp);
void scnFinalSuperComp(UnpStr *unp);
void scnIntros(UnpStr *unp);
void scnMasterCompressor(UnpStr *unp);
void scnMegabyte(UnpStr *unp);
void scnMrCross(UnpStr *unp);
void scnMrZ(UnpStr *unp);
void scnPuCrunch(UnpStr *unp);
void scnSection8(UnpStr *unp);
void scnTBCMultiComp(UnpStr *unp);
void scnTCScrunch(UnpStr *unp);
void scnTimeCruncher(UnpStr *unp);
void scnXTC(UnpStr *unp);

/* Start the emulation at `entry`, unless the program has a BASIC SYS line,
 * in which case the SYS knows better. */
inline void forceEntryIfNoSys(UnpStr *unp, int entry) {
	if (unp->_info->_run == -1)
		unp->_forced = entry;
}

/* For an intro or a virus in front of a BASIC program: when it is done it
 * jumps to the interpreter loop to RUN what it has moved back to $0801,
 * having set the end-of-program pointer at $2d. `exactReturn` if only a
 * jump to exactly that address counts as being done. */
inline void setBasicReturn(UnpStr *unp, bool exactReturn) {
	unp->_retAdr = kBasicRun;
	if (exactReturn)
		unp->_rtAFrc = 1;
	unp->_strMem = kBasicStart;
	unp->_endAdr = kZpVarTab;
}

} // End of namespace Unp64

#endif
