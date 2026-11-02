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

#include "scanners.h"

namespace Unp64 {

/* Action Replay and similar freeze-cartridge dumps.
 *
 * Ported from the original UNP64 Scn_AR. Only the single-file branches are
 * carried over here: the AR3/AR4/ARU freezer format, the snapshot freezer
 * ("SSNAP"), and the single-file Freeze Machine format. The two-file freeze
 * variants from upstream (which read a companion file off disk) are omitted,
 * as this cut-down build unpacks a single in-memory PRG. */
void scnAR(UnpStr *unp) {
	uint8_t *mem = unp->_mem;
	int q, p;
	if (unp->_depAdr == 0) {
		if (u32eq(mem + 0x80f, 0xDD0D8D7F) &&
			u32eq(mem + 0x927, 0x4CE7D0CA)) {
			for (q = 0xa00, p = 0; p < 0x100; p++) {
				if (mem[q + p] == 0x99 &&
					mem[q + p + 1] == mem[0x092b] &&
					mem[q + p + 2] == mem[0x092c] &&
					mem[q + p + 0x11] == 0x40) { /* Action Replay v4 */
					unp->_depAdr = 0x100 + p;
					break;
				}
				if (mem[q + p] == 0x99 &&
					mem[q + p + 1] == mem[0x092b] &&
					mem[q + p + 2] == mem[0x092c] &&
					mem[q + p + 0x10] == 0x40) { /* Action Replay v3.x */
					unp->_depAdr = 0x100 + p;
					break;
				}
			}
			if (!unp->_depAdr) { /* Action Replay, unknown variant */
				unp->_depAdr = READ_LE_UINT16(&mem[0x092b]);
			}
			unp->_rtiFrc = 1;
			forceEntryIfNoSys(unp, 0x080d);
		} else if (u32eq(mem + 0x812, 0xDD0D8DDC)) { /* Super Snapshot */
			for (p = 0x900; p < 0xcfff; p++) {
				if (u32eq(mem + p, 0xA9DC0E8D) &&
					u32eq(mem + p + 0x0a, 0xA9DD0E8D) &&
					mem[p + 0x18] == 0x4c) {
					unp->_depAdr = READ_LE_UINT16(&mem[p + 0x19]);
					unp->_rtiFrc = 1;
					forceEntryIfNoSys(unp, 0x080d);
					break;
				}
			}
		} else if (u32eq(mem + 0x80f, 0xDD0D8D7F) &&
				u32eq(mem + 0x8ef, 0x0330BD01) &&
				u32eq(mem + 0x8fa, 0x7E4C00A0) &&
				u32eq(mem + 0xbc8, 0x4C01C6DF)) { /* Freeze Machine */
			unp->_depAdr = READ_LE_UINT16(&mem[0x0bcc]);
			unp->_strMem = 2;
			unp->_endAdr = 0x10000;
			unp->_rtiFrc = 1;
			forceEntryIfNoSys(unp, 0x080d);
		}
	}
	if (unp->_depAdr != 0) {
		unp->_idFlag = 1;
		return;
	}
}

} // End of namespace Unp64
