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

#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>

#include "6502_emu.h"
#include "exo_util.h"
#include "unp64.h"

#include "unp64_interface.h"

namespace Unp64 {

/* C64 addresses that the driver knows by name. */

/* Zero page. */
constexpr int kZpCpuPort = 0x01; /* 6510 port: which ROMs and I/O are banked in */
constexpr int kZpTxtTab = 0x2b;  /* start of BASIC text */
constexpr int kZpAryTab = 0x2f;  /* start of arrays */
constexpr int kZpStrEnd = 0x31;  /* end of arrays */
constexpr int kZpCurLin = 0x39;  /* current BASIC line number */
constexpr int kZpFnLen = 0xb7;   /* file name length (SETNAM) */
constexpr int kZpFnAdr = 0xbb;   /* file name address (SETNAM) */
constexpr int kZpEal = 0xae;     /* end address of the last LOAD */

constexpr int kInputBuffer = 0x200; /* BASIC line input buffer */
constexpr int kVectors = 0x314;     /* IRQ, BRK, NMI and KERNAL I/O vectors */
constexpr int kScreenRam = 0x400;
constexpr int kCartridge = 0x8000;  /* cold start vector, then "CBM80" at +4 */

/* BASIC ROM, $A000-$BFFF. */
constexpr int kBasicRom = 0xa000;
constexpr int kBasicRomEnd = 0xbfff;

/* KERNAL ROM, $E000-$FFFF. */
constexpr int kKernalRom = 0xe000;
constexpr int kKernalClrscrA = 0xe536; /* two entries into the clear-screen code */
constexpr int kKernalClrscrB = 0xe544;
constexpr int kKernalIrq = 0xea31;     /* the IRQ handler, to kKernalIrqEnd */
constexpr int kKernalIrqEnd = 0xeb76;
constexpr int kKernalGetChar = 0xf13e; /* what GETIN jumps to for the keyboard */
constexpr int kKernalLoadSerial = 0xf4a2;
constexpr int kKernalReset = 0xfce2;
constexpr int kKernalRestor = 0xfd15;
constexpr int kKernalIoinit = 0xfda3;
constexpr int kKernalCint = 0xff5b;
constexpr int kKernalSetnam = 0xffbd;
constexpr int kKernalChrout = 0xffd2;
constexpr int kKernalLoad = 0xffd5;
constexpr int kKernalGetin = 0xffe4;

constexpr int kOpRts = 0x60;
constexpr int kOpRti = 0x40;

/* The emulated RAM is 64 KiB plus this much zeroed slack. Scanners match
 * signatures at `mem + p + offset` with p anywhere up to $FFFF, and the
 * emulator reads the operand bytes of an instruction at $FFFF: both run off
 * the end of the address space, and land here. */
constexpr size_t kMemSize = 0x10000;
constexpr size_t kMemSlack = 0x1000;

/* Switch strings: at most this many switches, each shorter than this. */
constexpr int kMaxSwitches = 4;
constexpr size_t kMaxSwitchLength = 64;

/* Snapshot of the IRQ/NMI/BRK/IO vectors at $0314-$0333 as they
 * appear on a freshly-reset C64. Some packers decrypt themselves
 * using these bytes, so we must seed them rather than leave zeros. */
static const uint8_t kInitialVector[0x20] = {0x31, 0xEA, 0x66, 0xFE, 0x47, 0xFE, 0x4A, 0xF3,
					 0x91, 0xF2, 0x0E, 0xF2, 0x50, 0xF2, 0x33, 0xF3,
					 0x57, 0xF1, 0xCA, 0xF1, 0xED, 0xF6, 0x3E, 0xF1,
					 0x2F, 0xF3, 0x66, 0xFE, 0xA5, 0xF4, 0xED, 0xF5};

/* Snapshot of page $0100 (the 6502 stack page) immediately after a
 * BASIC `SYS` from the READY prompt — some packers expect leftover
 * bytes from BASIC's tokenized SYS line to still be on the stack. */
static const uint8_t kStackAfterSys[0x100] = {0x33, 0x38, 0x39, 0x31, 0x31, 0x00, 0x30, 0x30, 0x30, 0x30,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
					 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7D,
					 0xEA, 0x00, 0x00, 0x82, 0x22, 0x0E, 0xBC, 0x81, 0x64, 0xB8,
					 0x0C, 0xBD, 0xBA, 0xB7, 0xBC, 0x03, 0x00, 0x46, 0xE1, 0xE9,
					 0xA7, 0xA7, 0x79, 0xA6, 0x9C, 0xE3};

/* The state that a pass begins with. `u` is all zeros on entry; these are
 * the fields that are not zero by default. They assume a BASIC-style PRG
 * loaded at $0801 with a SYS line transferring control around $0800. */
static void startPass(UnpStr &u, uint8_t *mem, LoadInfo *info) {
	u._strMem = 0x800;
	u._retAdr = 0x800;
	u._endAdr = 0x10000;
	u._keypressIdx = -1;
	u._fStack = 1;
	u._mem = mem;
	u._info = info;
	*info = {};
}

/* True if pc is one of the BASIC ROM entry points that signal
 * "return to BASIC and RUN the program" — i.e. the unpacker has
 * finished and is handing control back to a fresh SYS/RUN. */
static bool isBasicRun1(int pc) {
	return pc == kBasicRun || pc == 0xa7ea || pc == 0xa7b1 || pc == 0xa474 || pc == 0xa533 || pc == 0xa871 || pc == 0xa888 || pc == 0xa8bc;
}

/* Broader match for "we're now executing in BASIC's interpreter
 * dispatch loop", which also indicates the unpacker has finished. */
static bool isBasicRun2(int pc) {
	return isBasicRun1(pc) || (pc >= 0xa57c && pc <= 0xa659) || pc == 0xa660 || pc == 0xa68e;
}

/* The low three bits of the CPU port say what is banked in. */
static int bankBits(const uint8_t *mem) {
	return mem[kZpCpuPort] & 7;
}

/* Bounce out of ROM code that is not emulated: point the CPU at an RTS. */
static void forceRts(CpuCtx *r, uint8_t *mem) {
	mem[0] = kOpRts;
	r->_pc = 0;
}

static void setFlagsNz(CpuCtx *r, uint8_t value) {
	r->_flags &= ~(128 | 2);
	r->_flags |= (value == 0 ? 2 : 0) | (value & 128);
}

static int readWord(const uint8_t *mem, int addr) {
	return mem[addr] | mem[addr + 1] << 8;
}

/* Split a UNP64 switch string (e.g. "-d0x8000 -t0x9fff -c") into argv-style
 * tokens. Returns the number of tokens written into settings[], or -1 if
 * the string does not fit: a fifth switch, or one of kMaxSwitchLength
 * characters or more. That is a failure and not a truncation, because a
 * switch that silently went missing shows up as a game that unpacks to
 * garbage, and nothing points back to the string. */
static int parseSwitchString(const char *switches, char settings[kMaxSwitches][kMaxSwitchLength]) {
	if (switches == nullptr)
		return 0;

	int numSettings = 0;
	const char *p = switches;
	while (true) {
		while (*p == ' ')
			p++;
		if (*p == '\0')
			return numSettings;
		size_t tokenLength = strcspn(p, " ");
		if (numSettings == kMaxSwitches || tokenLength >= kMaxSwitchLength)
			return -1;
		memcpy(settings[numSettings], p, tokenLength);
		settings[numSettings][tokenLength] = '\0';
		numSettings++;
		p += tokenLength;
	}
}

/* Interpret parsed switches and mutate the unpacker state accordingly.
 * Starts from `startIdx` so the caller can pre-consume the leading -f.
 * Returns false if a switch that takes a number has something else after
 * the letter. Tokens that are no switch, and switch letters that this
 * cut-down version does not know, are skipped as they always were. */
static bool applySwitches(UnpStr &u, char settings[kMaxSwitches][kMaxSwitchLength], int numSettings, int startIdx,
                          const LoadInfo *info, int *iterMax) {
	for (int p = startIdx; p < numSettings; p++) {
		if (settings[p][0] != '-')
			continue;
		const char *number = settings[p] + 2;
		switch (settings[p][1]) {
		case '-':
			/* "--" terminates option parsing. */
			return true;
		case 'e':
			/* -e<addr>: force entry point (override BASIC SYS). */
			if (!strToInt(number, &u._forced))
				return false;
			u._forced &= 0xffff;
			break;
		case 'a':
			/* -a: dump the full 64K, ignoring any detected
			 * start/end hints from scanners. */
			u._strMem = 2;
			u._endAdr = 0x10001;
			u._fEndAf = 0;
			u._fStrAf = 0;
			u._strAdC = 0;
			u._endAdC = 0;
			u._monEnd = 0;
			u._monStr = 0;
			break;
		case 'r':
			/* -r<addr>: emulation halts when pc >= addr. */
			if (!strToInt(number, &u._retAdr))
				return false;
			u._retAdr &= 0xffff;
			break;
		case 'R':
			/* -R<addr>: like -r but require an exact pc match. */
			if (!strToInt(number, &u._retAdr))
				return false;
			u._retAdr &= 0xffff;
			u._rtAFrc = 1;
			break;
		case 'd':
			/* -d<addr>: depack-done address; first loop exits when pc == addr. */
			if (!strToInt(number, &u._depAdr))
				return false;
			u._depAdr &= 0xffff;
			break;
		case 't':
			/* -t<addr>: explicit end-of-data address for output. */
			if (!strToInt(number, &u._endAdr))
				return false;
			u._endAdr &= 0xffff;
			if (u._endAdr >= 0x100)
				u._endAdr++;
			break;
		case 'u':
			/* -u: zero out bytes that the unpacker did not modify. */
			u._wrMemF = 1;
			break;
		case 'l':
			/* -l: trim a trailing tail of unchanged bytes from the dump. */
			u._lfMemF = info->_end;
			break;
		case 's':
			/* -s: don't pre-seed the stack page with the SYS snapshot. */
			u._fStack = 0;
			break;
		case 'c':
			/* -c: re-run unpacker on the output (handles nested packers). */
			u._recurs++;
			break;
		case 'm': // keep undocumented for now
			/* -m<n>: override the per-loop instruction cap. */
			if (!strToInt(number, iterMax))
				return false;
			break;
		}
	}
	return true;
}

/* Clamp the end address to a sane range: (start, $10000]. */
static void clampEndAddress(UnpStr &u) {
	if (u._endAdr <= 0 || u._endAdr > 0x10000 || u._endAdr < u._strMem)
		u._endAdr = 0x10000;
}

/* After both emulation phases complete, fold any pending end/start address
 * adjustments (constants and register-derived offsets) into _endAdr/_strMem. */
static void applyEndStartOffsets(UnpStr &u, const CpuCtx *r) {
	const struct {
		unsigned int flag;
		uint8_t value;
	} regs[] = {{EA_USE_A, r->_a}, {EA_USE_X, r->_x}, {EA_USE_Y, r->_y}};

	if (u._endAdC & 0xffff)
		u._endAdr = (u._endAdr + (u._endAdC & 0xffff)) & 0xffff;
	for (const auto &reg : regs) {
		if (u._endAdC & reg.flag)
			u._endAdr = (u._endAdr + reg.value) & 0xffff;
	}

	if (u._strAdC & 0xffff) {
		u._strMem = (u._strMem + (u._strAdC & 0xffff)) & 0xffff;
		/* only if ea_addff, no reg involved */
		if (((u._strAdC & 0xffff0000) == EA_ADDFF) && ((u._strMem & 0xff) == 0))
			u._strMem = (u._strMem + 0x100) & 0xffff;
	}
	for (const auto &reg : regs) {
		if (!(u._strAdC & reg.flag))
			continue;
		u._strMem = (u._strMem + reg.value) & 0xffff;
		if (u._strAdC & EA_ADDFF) {
			if ((u._strMem & 0xff) == 0xff)
				u._strMem++;
			if (reg.value == 0)
				u._strMem = (u._strMem + 0x100) & 0xffff;
		}
	}
}

/* Seed the emulated C64 RAM and CPU registers so the depacker starts in
 * the state it would expect right after a BASIC `SYS <run>`: zero-page
 * BASIC pointers, the stack page snapshot, the IRQ vector table, and
 * sane CPU register/flag values. */
static void seedC64State(const UnpStr &u, CpuCtx *r, uint8_t *mem, const LoadInfo *info) {
	/* $0000 = RTS so a stray jump to zero unwinds cleanly. $0001 = bank
	 * latch ($37 = ROMs+I/O visible; $38 = RAM exposed under ROM regions
	 * when the forced entry sits in a ROM area). */
	mem[0] = kOpRts;
	r->_cycles = 0;
	mem[kZpCpuPort] = 0x37;

	if (((u._forced >= 0xa000) && (u._forced < 0xc000)) || (u._forced >= 0xd000))
		mem[kZpCpuPort] = 0x38;

	/* TXTTAB / VARTAB / ARYTAB / STREND — packers often read these. */
	int varStart = info->_basicVarStart == -1 ? info->_end : info->_basicVarStart;
	mem[kZpTxtTab] = info->_basicTxtStart & 0xff;
	mem[kZpTxtTab + 1] = info->_basicTxtStart >> 8;
	for (int ptr : {kZpVarTab, kZpAryTab, kZpStrEnd}) {
		mem[ptr] = varStart & 0xff;
		mem[ptr + 1] = varStart >> 8;
	}
	mem[kZpEal] = info->_end & 0xff;
	mem[kZpEal + 1] = info->_end >> 8;

	/* CCS unpacker requires $39/$3a (current basic line number) set. */
	mem[kZpCurLin] = mem[kBasicStart + 2];
	mem[kZpCurLin + 1] = mem[kBasicStart + 3];
	mem[0x52] = 0;
	mem[0x53] = 3;

	if (u._fStack) {
		memcpy(mem + 0x100, kStackAfterSys, sizeof kStackAfterSys);
		r->_sp = 0xf6; /* sys from immediate mode leaves $f6 in stackptr */
	} else {
		r->_sp = 0xff;
	}

	/* Some packers use IRQ vector bytes to decrypt — only stamp them in
	 * if the PRG payload itself doesn't already occupy that region. */
	if (info->_start > (int)(kVectors + sizeof kInitialVector))
		memcpy(mem + kVectors, kInitialVector, sizeof kInitialVector);

	mem[kInputBuffer] = 0x8a; /* TXA — what the BASIC SYS evaluator leaves in $0200 */
	r->_mem = mem;
	r->_pc = info->_run;
	r->_flags = 0x20;
	r->_a = 0;
	r->_y = 0;
	/* X is cleared too, unless the entry point is at $0351 or below
	 * (iAN: "temporary for XIP"). Then it keeps what it had: 0 on the first
	 * pass, as unp64cpp() starts from a zeroed context, and whatever the
	 * layer before left in it on a recursion pass. */
	if (info->_run > 0x351)
		r->_x = 0;
}

/* The two KERNAL calls that both phases of the emulation fake, because
 * they leave something in memory that a depacker reads back. The caller
 * has checked that the program counter is in the KERNAL ROM. */
static void stubKernalState(CpuCtx *r, uint8_t *mem) {
	/* SETNAM ($FFBD): persist filename pointer/length in zeropage. */
	if (r->_pc == kKernalSetnam) {
		mem[kZpFnLen] = r->_a;
		mem[kZpFnAdr] = r->_x;
		mem[kZpFnAdr + 1] = r->_y;
	}
	/* IOINIT ($FDA3): mimic the post-init CIA/$01 state. */
	if (r->_pc == kKernalIoinit) {
		mem[kZpCpuPort] = 0xe7;
		r->_a = 0xd7;
		r->_x = 0xff;
	}
}

/* Phase 1 of emulation: step the 6502 until pc reaches the depack-done
 * address (or, if none is configured, until pc passes the return address).
 * Most of the body is "the program just called a KERNAL/BASIC ROM
 * routine" handling — we don't emulate ROM code, so we fake observable
 * side effects and bounce execution back out with a synthesized RTS.
 *
 * Returns true if the loop exited normally, false on a fatal abort
 * (illegal opcode or iteration cap). */
static bool emulateUntilDepack(UnpStr &u, CpuCtx *r, uint8_t *mem, LoadInfo *info, int iterMax) {
	int iter = 0;
	while (u._depAdr ? r->_pc != u._depAdr : r->_pc >= u._retAdr) {
		/* Executing inside ROM (BASIC at $A000-BFFF or KERNAL at $E000+)? */
		if ((bankBits(mem) >= 6 && r->_pc >= kKernalRom) || (r->_pc >= kBasicRom && r->_pc <= kBasicRomEnd && bankBits(mem) > 6)) {
			/* some packer relies on regs set at return from CLRSCR */
			if (r->_pc == kKernalClrscrA || r->_pc == kKernalClrscrB || r->_pc == kKernalCint || (r->_pc == kKernalChrout && r->_a == 0x93)) {
				if (r->_pc != kKernalChrout) {
					r->_x = 0x01;
					r->_y = 0x84;
					if (r->_pc == kKernalCint)
						r->_a = 0x97; /* actually depends on $d012 */
					else
						r->_a = 0xd8;
					setFlagsNz(r, r->_a);
				}
				memset(mem + kScreenRam, 0x20, 1000);
			}
			/* intros: GETIN ($FFE4) / GET-CHAR ($F13E). Cycle through a
			 * canned sequence of keypresses (space, N, Ctrl-C, _, cursor
			 * down, return, 1) so any keyboard-driven intro/menu advances. */
			if (r->_pc == kKernalGetin || r->_pc == kKernalGetChar) {
				static const uint8_t fpressedchars[] = {0x20, 0, 0x4e, 0, 3, 0, 0x5f, 0, 0x11, 00, 0x0d, 0, 0x31, 0};
				u._keypressIdx = (u._keypressIdx + 1) % (int)sizeof fpressedchars;
				r->_a = fpressedchars[u._keypressIdx];
				setFlagsNz(r, r->_a);
			}
			/* RESTOR ($FD15): fake registers normally set by the KERNAL
			 * vector restore so packers reading them get plausible values. */
			if (r->_pc == kKernalRestor) {
				r->_a = 0x31;
				r->_x = 0x30;
				r->_y = 0xff;
			}
			stubKernalState(r, mem);
			/* LOAD ($FFD5) / KERNAL serial bus routine ($F4A2):
			 * can't service real I/O — treat hitting these as "done". */
			if (r->_pc == kKernalLoad || r->_pc == kKernalLoadSerial)
				break;
			/* Landed in BASIC's RUN/READY path — look for the new SYS line
			 * and jump there; otherwise fall through with an RTS. */
			if (isBasicRun1(r->_pc)) {
				info->_run = findSys(mem + info->_basicTxtStart, kMemSize - info->_basicTxtStart);
				if (info->_run > 0) {
					r->_sp = 0xf6;
					r->_pc = info->_run;
				} else {
					forceRts(r, mem);
				}
			} else {
				forceRts(r, mem);
			}
		}

		/* Step the emulated CPU one instruction. */
		if (nextInst(r))
			return false;

		if (++iter == iterMax)
			return false;

		/* Exomizer family: while the decompressor runs its code-on-stack
		 * tail at $100-$200, latch the end address out of $FE/$FF. */
		if (u._exoFnd && (u._endAdr == 0x10000) && (r->_pc >= 0x100) && (r->_pc <= 0x200) && (u._strMem != 2)) {
			u._endAdr = mem[0xfe] + (mem[0xff] << 8);
			if ((u._exoFnd & 0xff) == 0x30) {
				u._endAdr = (u._exoFnd >> 8) + (mem[0xff] << 8);
			} else if ((u._exoFnd & 0xff) == 0x32) {
				u._endAdr = 1 + ((u._exoFnd >> 8) + (mem[0xff] << 8));
			}
			if (u._endAdr == 0)
				u._endAdr = 0x10001;
		}

		/* Latch end/start addresses from RAM the instant we hit _depAdr. */
		if (u._fEndBf && (u._endAdr == 0x10000) && (r->_pc == u._depAdr)) {
			u._endAdr = readWord(mem, u._fEndBf);
			u._endAdr++;
			if (u._endAdr == 0)
				u._endAdr = 0x10001;
			u._fEndBf = 0;
		}
		if (u._fStrBf && (u._strMem != 0x2) && (r->_pc == u._depAdr)) {
			u._strMem = readWord(mem, u._fStrBf);
			u._fStrBf = 0;
		}
	}
	return true;
}

/* Phase 2 of emulation: depacker has written the payload to RAM and is
 * now in its "transition to the depacked program" tail. Keep stepping
 * until pc reaches (or passes) the configured return address so any
 * final fix-ups land in RAM before we dump it. */
static bool emulateUntilReturn(UnpStr &u, CpuCtx *r, uint8_t *mem, int iterMax) {
	int iter = 0;
	while (u._rtAFrc ? r->_pc != u._retAdr : r->_pc < u._retAdr) {
		/* Each time we re-pass the depack address, sample the watched
		 * "current write pointer" location; track the max-end / min-start. */
		if (u._monEnd && r->_pc == u._depAdr) {
			int p = mem[u._monEnd >> 16] | mem[u._monEnd & 0xffff] << 8;
			if (p > (u._endAdr & 0xffff))
				u._endAdr = p;
		}
		if (u._monStr && r->_pc == u._depAdr) {
			int p = mem[u._monStr >> 16] | mem[u._monStr & 0xffff] << 8;
			if (p > 0) {
				if (u._mon1st == 0) {
					u._strMem = p;
				}
				u._mon1st = (unsigned int)u._strMem;
				u._strMem = (p < u._strMem ? p : u._strMem);
			}
		}

		/* Don't actually run KERNAL code — bounce out via synthesized RTS. */
		if (r->_pc >= kKernalRom && bankBits(mem) >= 6)
			forceRts(r, mem);

		if (nextInst(r))
			return false;

		/* Scanner-detected RTI exit: step the RTI and treat its target as
		 * the return address. */
		if ((mem[r->_pc] == kOpRti) && (u._rtiFrc == 1)) {
			if (nextInst(r))
				return false;
			u._retAdr = r->_pc;
			u._rtAFrc = 1;
			if (u._retAdr < u._strMem)
				u._strMem = 2;
			break;
		}

		if (++iter == iterMax)
			return false;

		/* Reached BASIC ROM with BASIC banked in — likely returning to RUN. */
		if (r->_pc >= kBasicRom && r->_pc <= kBasicRomEnd && bankBits(mem) == 7) {
			if (isBasicRun2(r->_pc)) {
				r->_pc = kBasicRun;
				break;
			}
			forceRts(r, mem);
		}

		if (r->_pc >= kKernalRom && bankBits(mem) >= 6) {
			stubKernalState(r, mem);
			/* Return into IRQ handler — stop here. */
			if ((r->_pc >= kKernalIrq && r->_pc <= kKernalIrqEnd) || r->_pc == kKernalLoad || r->_pc == kKernalReset)
				break;
			forceRts(r, mem);
		}
	}
	return true;
}

/* Load the program, find where it starts, let the scanners identify the
 * packer and apply the switches. False if there is nothing to run. */
static bool loadAndIdentify(UnpStr &u, LoadInfo *info, uint8_t *mem, uint8_t *oldmem,
                            const uint8_t *program, size_t length,
                            char settings[kMaxSwitches][kMaxSwitchLength], int numSettings, int firstSwitch,
                            int *iterMax) {
	info->_basicTxtStart = kBasicStart;
	/* Load the PRG into the emulated address space (honours the
	 * two-byte load address header) and extract metadata. */
	if (!loadData(program, length, mem, info))
		return false;
	/* no start address from load */
	if (info->_run == -1) {
		/* look for sys line */
		info->_run = findSys(mem + info->_basicTxtStart, kMemSize - info->_basicTxtStart);
	}

	/* Scan the loaded image for known packer signatures, which
	 * populates _depAdr/_endAdr/_strMem hints and may set _idFlag. */
	scanners(&u);

	/* Apply user-supplied switches, but only on the first pass — when we
	 * recurse on a nested payload, the inner pass should use whatever the
	 * scanners detected, not the outer command-line overrides. */
	if (u._recurs == 0 && numSettings > 0) {
		if (!applySwitches(u, settings, numSettings, firstSwitch, info, iterMax))
			return false;
	}

	/* Identify-only mode: bail out early if no depack address was found. */
	if (u._idOnly && u._depAdr == 0)
		return false;

	/* For diff-based output trimming (-u/-l), snapshot RAM before run. */
	if (u._wrMemF || u._lfMemF)
		memcpy(oldmem, mem, kMemSize);

	if (u._forced)
		info->_run = u._forced;

	if (info->_run == -1)
		return false;

	if (u._strMem > u._retAdr)
		u._strMem = u._retAdr;

	return true;
}

/* The emulation has stopped. Read the start and end of the unpacked
 * program out of the places the scanner said they would be in by now, and
 * follow the program's own way back to BASIC or to a cartridge. */
static void resolveEndStart(UnpStr &u, CpuCtx *r, LoadInfo *info, const uint8_t *mem) {
	/* "Read end-address after depack-done from this location." Resolved
	 * now because the value at _fEndAf is only correct once execution
	 * has reached the post-depack return path. */
	if (u._fEndAf && u._monEnd) {
		int end = readWord(mem, u._fEndAf);
		if (end > u._endAdr)
			u._endAdr = end;

		u._endAdC = 0;
		u._fEndAf = 0;
	}

	if (u._fEndAf && (u._endAdr == 0x10000)) {
		u._endAdr = readWord(mem, u._fEndAf);
		if (u._endAdr == 0)
			u._endAdr = 0x10000;
		else
			u._endAdr++;
		u._fEndAf = 0;
	}

	if (u._fStrAf) {
		u._strMem = readWord(mem, u._fStrAf);
		u._strMem++;
		u._fStrAf = 0;
	}

	/* Exomizer leaves the final start address in $FE/$FF, optionally
	 * adjusted by Y depending on the addressing-mode variant detected. */
	if (u._exoFnd && (u._strMem != 2)) {
		u._strMem = mem[0xfe] + (mem[0xff] << 8);

		if ((u._exoFnd & 0xff) == 0x30) {
			u._strMem += r->_y;
		} else if ((u._exoFnd & 0xff) == 0x32) {
			u._strMem += r->_y + 1;
		}
	}

	/* Stopped at the KERNAL reset vector ($FCE2) with the C64 cartridge
	 * autostart signature ("CBM80") at $8004-$8008? Then follow the
	 * cartridge's cold-start pointer. Otherwise, if we wound up at the
	 * BASIC RUN entry, look up the freshly-installed SYS line. */
	if (r->_pc == kKernalReset) {
		if (u32eq(mem + kCartridge + 4, 0x38cdc2c3) && mem[kCartridge + 8] == 0x30) {
			r->_pc = READ_LE_UINT16(&mem[kCartridge]);
		}
	} else if (r->_pc == kBasicRun) {
		info->_basicTxtStart = readWord(mem, kZpTxtTab);
		if (info->_basicTxtStart == kBasicStart) {
			info->_run = findSys(mem + info->_basicTxtStart, kMemSize - info->_basicTxtStart);
			if (info->_run > 0)
				r->_pc = info->_run;
		}
	}
}

/* The -u and -l switches: clear what the depacker left as it found it. */
static void applyDiffTrim(UnpStr &u, uint8_t *mem, const uint8_t *oldmem) {
	/* -u: zero any 4-byte aligned region that the unpacker didn't change
	 * compared to the pre-run snapshot — strips out leftover filler from
	 * the compressed image so only the truly-written bytes remain. */
	if (u._wrMemF) {
		u._wrMemF = 0;
		for (int addr = 0x800; addr < 0x10000; addr += 4) {
			if (memcmp(oldmem + addr, mem + addr, 4) == 0) {
				memset(mem + addr, 0, 4);
				u._wrMemF = 1;
			}
		}
		/* clean also the $fd30 table copy in RAM */
		if (memcmp(mem + 0xfd30, kInitialVector, sizeof kInitialVector) == 0) {
			memset(mem + 0xfd30, 0, sizeof kInitialVector);
		}
	}

	/* -l: scan back from $FFFF zeroing bytes that match the original
	 * compressed image, stopping at the first divergence — trims any
	 * tail of unchanged bytes that the depacker happened to leave alone.
	 * Running out of image counts as a divergence. */
	if (u._lfMemF) {
		for (int addr = 0xffff; addr > 0x0800; addr--) {
			if (u._lfMemF > 0 && oldmem[--u._lfMemF] == mem[addr])
				mem[addr] = 0x0;
			else {
				if (addr >= 0xffff)
					u._lfMemF = u._ecaFlg;
				break;
			}
		}
	}
}

/* Settle the start and end address of the unpacked program. */
static void finalizeRange(UnpStr &u, const CpuCtx *r, uint8_t *mem) {
	/*  endadr is set to a ZP location? then use it as a pointer
	  todo: use __fEndAf instead, it can be used for any location, not only ZP. */
	if (u._endAdr && (u._endAdr < 0x100))
		u._endAdr = readWord(mem, u._endAdr);

	/* "Extra copy after" flag: some packers move a $1000-byte block out
	 * of the I/O space ($D000-DFFF) into RAM at the end — account for
	 * that extra range so the dump covers the relocated copy too. */
	if (u._ecaFlg && (u._strMem != 2)) /* checkme */ {
		int relocated = (u._ecaFlg >> 16) & 0xffff;
		if (u._endAdr >= relocated) {
			/* most of the times transfers $2000 byte from $d000-efff to $e000-ffff but there are exceptions */
			if (u._lfMemF)
				memset(mem + relocated, 0, 0x1000);
			u._endAdr += 0x1000;
		}
	}

	/* Clamp the end address to a sane range before applying offsets. */
	clampEndAddress(u);

	/* _endAdC / _strAdC encode "after the run, add this constant and/or
	 * the value of this register to the end/start address" — used when
	 * the depacker computes its final pointer in registers rather than
	 * leaving it in a known memory cell. */
	applyEndStartOffsets(u, r);

	/* The load address goes in the two bytes in front of the start. */
	if (u._strMem < 2)
		u._strMem = 2;
	clampEndAddress(u);
}

/* Write the standard PRG two-byte load-address header just before the
 * unpacked data, then copy [header .. end) to the caller's buffer.
 * Returns false if the result wouldn't fit. */
static bool writeUnpackedOutput(uint8_t *mem, int strMem, int endAdr,
                                uint8_t *destinationBuffer,
                                size_t destBufferCapacity,
                                size_t *finalLength) {
	size_t outSize = (size_t)(endAdr - strMem + 2);
	if (outSize > destBufferCapacity)
		return false;
	mem[strMem - 2] = strMem & 0xff;
	mem[strMem - 1] = strMem >> 8;
	memcpy(destinationBuffer, mem + (strMem - 2), outSize);
	*finalLength = outSize;
	return true;
}

/* Core unpacker entry point: see unp64.h.
 *
 * The strategy is to emulate the 6502 CPU running the packed program
 * inside a 64 KiB RAM image until execution reaches a known "depack done"
 * address, then copy the decompressed bytes from emulator RAM to
 * destinationBuffer. All state is on this function's stack or owned by
 * it, so calls on different threads do not get in each other's way. */
bool unp64cpp(const uint8_t *compressed, size_t length, uint8_t *destinationBuffer, size_t destBufferCapacity, size_t *finalLength, const char *switches) {
	char settings[kMaxSwitches][kMaxSwitchLength];
	int numSettings = parseSwitchString(switches, settings);
	if (numSettings < 0)
		return false;

	CpuCtx r = {};     /* 6502 CPU register/flag context */
	LoadInfo info = {}; /* parsed metadata about the loaded PRG */

	/* Emulated C64 RAM and a pre-run snapshot used by -u/-l. Heap-allocated
	 * to keep the 128 KiB out of the stack frame. */
	auto memBuf    = std::unique_ptr<uint8_t[]>(new uint8_t[kMemSize + kMemSlack]());
	auto oldmemBuf = std::unique_ptr<uint8_t[]>(new uint8_t[kMemSize]());
	uint8_t *mem    = memBuf.get();
	uint8_t *oldmem = oldmemBuf.get();

	int iterMax = ITERMAX; /* hard cap on emulated instructions per phase */

	UnpStr u = {};
	startPass(u, mem, &info);

	/* The -f<value> switch pre-fills the upper part of RAM with a byte
	 * pattern; some packers leave the unfilled region untouched and a
	 * known pattern makes "what was modified" detection (-u) reliable.
	 * The value is <address> << 16 | <byte>. If present, -f must come
	 * first; subsequent switches start at `firstSwitch`. */
	int firstSwitch = 0;
	if (numSettings != 0 && settings[0][0] == '-' && settings[0][1] == 'f') {
		int filler = 0;
		if (!strToInt(settings[0] + 2, &filler))
			return false;
		u._filler = (unsigned int)filler;
		if (u._filler) {
			memset(mem + (u._filler >> 16), u._filler & 0xff, kMemSize - (u._filler >> 16));
		}
		firstSwitch = 1;
	}

	/* More than one pass if the output is to be unpacked again: the -c
	 * switch, or a scanner that knows its packer comes in layers and has
	 * set _recurs. Every pass after the first loads what the one before
	 * produced, as the original UNP64 did by reading back the file that it
	 * had just written. If such a pass gets nowhere, the output is what
	 * the last good one made of it. */
	const uint8_t *input = compressed;
	size_t inputLength = length;
	for (int pass = 0;; pass++) {
		bool ok = loadAndIdentify(u, &info, mem, oldmem, input, inputLength, settings, numSettings, firstSwitch, &iterMax);
		if (ok) {
			seedC64State(u, &r, mem, &info);
			ok = emulateUntilDepack(u, &r, mem, &info, iterMax) && emulateUntilReturn(u, &r, mem, iterMax);
		}
		if (ok) {
			resolveEndStart(u, &r, &info, mem);
			applyDiffTrim(u, mem, oldmem);
			finalizeRange(u, &r, mem);
			ok = writeUnpackedOutput(mem, u._strMem, u._endAdr, destinationBuffer, destBufferCapacity, finalLength);
		}
		if (!ok)
			return pass > 0;

		/* _recurs counts up to RECUMAX from wherever it was set: 1 by -c,
		 * RECUMAX - 2 by a scanner that wants exactly three passes. The
		 * pass count is there for a scanner that sets it on every pass. */
		if (u._recurs == 0 || ++u._recurs > RECUMAX || pass + 1 >= RECUMAX)
			return true;
		/* Start over as a new call would, but for the count: nothing that
		 * the last layer's scanner or depacker left behind is of any use
		 * to the next one, and the switches were for the first only. */
		int recurs = u._recurs;
		u = {};
		startPass(u, mem, &info);
		u._recurs = recurs;
		r = {};
		memset(mem, 0, kMemSize + kMemSlack);
		input = destinationBuffer;
		inputLength = *finalLength;
	}
}

} // End of namespace Unp64


/* C-callable wrapper around Unp64::unp64cpp, so callers from C code (the
 * ScottFree, TaylorMade and UnQuill interpreters) don't need to know about
 * the namespace. See unp64_interface.h for the contract. */
int unp64(const uint8_t *compressed, size_t length, uint8_t *destinationBuffer, size_t *finalLength, const char *settings) {
	return Unp64::unp64cpp(compressed, length, destinationBuffer, UNP64_OUTPUT_CAPACITY, finalLength, settings);
}
