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

#include "unp64.h"

namespace Unp64 {

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

typedef void (*Scnptr)(UnpStr *);

/* Tried in this order until one sets _idFlag. The order is that of the
 * original UNP64, which lists them roughly by how often a packer is met;
 * it matters where two packers share a stub (the TimeCruncher clones that
 * the MasterCompressor and Mr. Cross scanners also know, for one), so do
 * not sort it. */
static const Scnptr kScanners[] = {
	scnAbuzeCrunch,
	scnByteBoiler,
	scnECA,
	scnExpert,
	scnAR,
	scnFinalSuperComp,
	scnIntros,
	scnCruel,
	scnPuCrunch,
	scnMasterCompressor,
	scnMrCross,
	scnMrZ,
	scnTCScrunch,
	scnTBCMultiComp,
	scnTimeCruncher,
	scnXTC,
	scnCCS,
	scnMegabyte,
	scnSection8,
	scnCaution,
	scnActionPacker,
	scnExomizer
};

void scanners(UnpStr *unp) {
	for (Scnptr scan : kScanners) {
		scan(unp);
		if (unp->_idFlag)
			break;
	}
}

} // End of namespace Unp64
