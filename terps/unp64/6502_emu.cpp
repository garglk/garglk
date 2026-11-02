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

#include "6502_emu.h"

namespace Unp64 {

#define FLAG_N 128
#define FLAG_V 64
#define FLAG_D 8
#define FLAG_I 4
#define FLAG_Z 2
#define FLAG_C 1

struct ArgEa {
	uint16_t _value;
};
struct ArgRelative {
	int8_t _value;
};
struct ArgImmediate {
	uint8_t _value;
};

union InstArg {
	ArgEa _ea;
	ArgRelative _rel;
	ArgImmediate _imm;
};

typedef void op_f(CpuCtx *r, int mode, InstArg *arg);
typedef int mode_f(CpuCtx *r, InstArg *arg);

/* One row of the opcode table. `_op` is null for the opcodes that are not
 * implemented. */
struct InstInfo {
	op_f *_op;
	mode_f *_mode;
	uint8_t _cycles;
};

#define MODE_IMMEDIATE 0
#define MODE_ZERO_PAGE 1
#define MODE_ZERO_PAGE_X 2
#define MODE_ZERO_PAGE_Y 3
#define MODE_ABSOLUTE 4
#define MODE_ABSOLUTE_X 5
#define MODE_ABSOLUTE_Y 6
#define MODE_INDIRECT 7
#define MODE_INDIRECT_X 8
#define MODE_INDIRECT_Y 9
#define MODE_RELATIVE 10
#define MODE_ACCUMULATOR 11
#define MODE_IMPLIED 12

static int mode_imm(CpuCtx *r, InstArg *arg) {
	arg->_imm._value = r->_mem[r->_pc + 1];
	r->_pc += 2;
	return MODE_IMMEDIATE;
}

static int mode_zp(CpuCtx *r, InstArg *arg) {
	arg->_ea._value = r->_mem[r->_pc + 1];
	r->_pc += 2;
	return MODE_ZERO_PAGE;
}

static int mode_zpx(CpuCtx *r, InstArg *arg) { /* iAN: ldx #1 lda $ff,x should fetch from $00 and not $100 */
	uint8_t lsbLo = (r->_mem[r->_pc + 1] + r->_x) & 0xff;
	arg->_ea._value = lsbLo;
	r->_pc += 2;
	return MODE_ZERO_PAGE_X;
}

static int mode_zpy(CpuCtx *r, InstArg *arg) { /* iAN: ldy #1 ldx $ff,y should fetch from $00 and not $100 */
	uint8_t lsbLo = (r->_mem[r->_pc + 1] + r->_y) & 0xff;
	arg->_ea._value = lsbLo;
	r->_pc += 2;
	return MODE_ZERO_PAGE_Y;
}

static int mode_abs(CpuCtx *r, InstArg *arg) {
	uint16_t offset = r->_mem[r->_pc + 1];
	uint16_t base = r->_mem[r->_pc + 2] << 8;
	arg->_ea._value = base + offset;
	r->_pc += 3;
	return MODE_ABSOLUTE;
}

static int mode_absx(CpuCtx *r, InstArg *arg) {
	uint16_t offset = r->_mem[r->_pc + 1] + r->_x;
	uint16_t base = r->_mem[r->_pc + 2] << 8;
	arg->_ea._value = base + offset;
	r->_pc += 3;
	r->_cycles += (offset > 255);
	return MODE_ABSOLUTE_X;
}

static int mode_absy(CpuCtx *r, InstArg *arg) {
	uint16_t offset = r->_mem[r->_pc + 1] + r->_y;
	uint16_t base = r->_mem[r->_pc + 2] << 8;
	arg->_ea._value = base + offset;
	r->_pc += 3;
	r->_cycles += (offset > 255);
	return MODE_ABSOLUTE_Y;
}

static int mode_ind(CpuCtx *r, InstArg *arg) {
	int lsbLo = r->_mem[r->_pc + 1];
	int msbLo = r->_mem[r->_pc + 2];
	int offsetlo = lsbLo | msbLo << 8;
	int offsethi = ((lsbLo + 1) & 0xff) | msbLo << 8;
	arg->_ea._value = r->_mem[offsetlo] | r->_mem[offsethi] << 8;

	r->_pc += 3;
	return MODE_INDIRECT;
}

static int mode_indx(CpuCtx *r, InstArg *arg) {
	uint8_t lsbLo = r->_mem[r->_pc + 1] + r->_x;
	uint8_t msbLo = lsbLo + 1;
	uint16_t base = r->_mem[msbLo] << 8;
	uint16_t offset = r->_mem[lsbLo];
	arg->_ea._value = base + offset;
	r->_pc += 2;
	return MODE_INDIRECT_X;
}

static int mode_indy(CpuCtx *r, InstArg *arg) {
	uint8_t lsbLo = r->_mem[r->_pc + 1];
	uint8_t msbLo = lsbLo + 1;
	uint16_t base = r->_mem[msbLo] << 8;
	uint16_t offset = r->_mem[lsbLo] + r->_y;
	arg->_ea._value = base + offset;
	r->_pc += 2;
	r->_cycles += (offset > 255);
	return MODE_INDIRECT_Y;
}

static int mode_rel(CpuCtx *r, InstArg *arg) {
	arg->_rel._value = (int8_t)r->_mem[r->_pc + 1];
	r->_pc += 2;
	return MODE_RELATIVE;
}

static int mode_acc(CpuCtx *r, InstArg *arg) {
	r->_pc++;
	return MODE_ACCUMULATOR;
}

static int mode_imp(CpuCtx *r, InstArg *arg) {
	r->_pc++;
	return MODE_IMPLIED;
}


static void updateFlagsNz(CpuCtx *r, uint8_t value) {
	r->_flags &= ~(FLAG_Z | FLAG_N);
	r->_flags |= (value == 0 ? FLAG_Z : 0) | (value & FLAG_N);
}

static void updateCarry(CpuCtx *r, int boolean) {
	r->_flags = (r->_flags & ~FLAG_C) | (boolean != 0 ? FLAG_C : 0);
}

static uint16_t subtract(CpuCtx *r, int carry, uint8_t val1, uint8_t value) {
	uint16_t target = val1 - value - (1 - !!carry);
	updateCarry(r, !(target & 256));
	updateFlagsNz(r, target & 255);
	return target;
}

static void update_overflow(CpuCtx *r, int boolean) {
	r->_flags = (r->_flags & ~FLAG_V) | (boolean != 0 ? FLAG_V : 0);
}

/* The operand of an instruction that reads: the byte after the opcode, or
 * the byte at the effective address. */
static uint8_t fetchOperand(CpuCtx *r, int mode, const InstArg *arg) {
	return mode == MODE_IMMEDIATE ? arg->_imm._value : r->_mem[arg->_ea._value];
}

/* What a shift or a rotate works on: the accumulator, or the byte at the
 * effective address. */
static uint8_t *rmwTarget(CpuCtx *r, int mode, const InstArg *arg) {
	return mode == MODE_ACCUMULATOR ? &r->_a : &r->_mem[arg->_ea._value];
}

/* Binary only, here and in SBC: the decimal flag can be set and cleared but
 * is never looked at. Depackers do not do sums in BCD. */
static void op_adc(CpuCtx *r, int mode, InstArg *arg) {
	uint16_t result;
	uint8_t value = fetchOperand(r, mode, arg);
	result = r->_a + value + (r->_flags & FLAG_C);
	updateCarry(r, result & 256);
	update_overflow(r, !((r->_a & 0x80) ^ (value & 0x80)) && ((r->_a & 0x80) ^ (result & 0x80)));
	r->_a = result & 0xff;
	updateFlagsNz(r, r->_a);
}

static void op_and(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	r->_a &= value;
	updateFlagsNz(r, r->_a);
}

static void op_asl(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t *valuep = rmwTarget(r, mode, arg);
	updateCarry(r, *valuep & 128);
	*valuep <<= 1;
	updateFlagsNz(r, *valuep);
}

static void branch(CpuCtx *r, InstArg *arg) {
	uint16_t target = r->_pc + arg->_rel._value;
	r->_cycles += 1 + ((target & ~255) != (r->_pc & ~255));
	r->_pc = target;
}

static void op_bcc(CpuCtx *r, int mode, InstArg *arg) {
	if (!(r->_flags & FLAG_C)) {
		branch(r, arg);
	}
}

static void op_bcs(CpuCtx *r, int mode, InstArg *arg) {
	if (r->_flags & FLAG_C) {
		branch(r, arg);
	}
}

static void op_beq(CpuCtx *r, int mode, InstArg *arg) {
	if (r->_flags & FLAG_Z) {
		branch(r, arg);
	}
}

static void op_bit(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags &= ~(FLAG_N | FLAG_V | FLAG_Z);
	r->_flags |= (r->_mem[arg->_ea._value] & FLAG_N) != 0 ? FLAG_N : 0;
	r->_flags |= (r->_mem[arg->_ea._value] & FLAG_V) != 0 ? FLAG_V : 0;
	r->_flags |= (r->_mem[arg->_ea._value] & r->_a) == 0 ? FLAG_Z : 0;
}

static void op_bmi(CpuCtx *r, int mode, InstArg *arg) {
	if (r->_flags & FLAG_N) {
		branch(r, arg);
	}
}

static void op_bne(CpuCtx *r, int mode, InstArg *arg) {
	if (!(r->_flags & FLAG_Z)) {
		branch(r, arg);
	}
}

static void op_bpl(CpuCtx *r, int mode, InstArg *arg) {
	if (!(r->_flags & FLAG_N)) {
		branch(r, arg);
	}
}

/* Pushes the return address and the flags, but does not load the program
 * counter from the vector at $fffe: there is no ROM behind it. The callers
 * of nextInst treat a BRK as the end of the run. */
static void op_brk(CpuCtx *r, int mode, InstArg *arg) {
	r->_mem[0x100 + r->_sp--] = (r->_pc + 1) >> 8;
	r->_mem[0x100 + r->_sp--] = r->_pc + 1;
	r->_mem[0x100 + r->_sp--] = r->_flags | 0x10;
}

static void op_bvc(CpuCtx *r, int mode, InstArg *arg) {
	if (!(r->_flags & FLAG_V)) {
		branch(r, arg);
	}
}

static void op_bvs(CpuCtx *r, int mode, InstArg *arg) {
	if ((r->_flags & FLAG_V)) {
		branch(r, arg);
	}
}

static void op_clc(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags &= ~FLAG_C;
}

static void op_cld(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags &= ~FLAG_D;
}

static void op_cli(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags &= ~FLAG_I;
}

static void op_clv(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags &= ~FLAG_V;
}


static void op_cmp(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	subtract(r, 1, r->_a, value);
}

static void op_cpx(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	subtract(r, 1, r->_x, value);
}

static void op_cpy(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	subtract(r, 1, r->_y, value);
}

static void op_dec(CpuCtx *r, int mode, InstArg *arg) {
	r->_mem[arg->_ea._value]--;
	updateFlagsNz(r, r->_mem[arg->_ea._value]);
}

static void op_dex(CpuCtx *r, int mode, InstArg *arg) {
	r->_x--;
	updateFlagsNz(r, r->_x);
}

static void op_dey(CpuCtx *r, int mode, InstArg *arg) {
	r->_y--;
	updateFlagsNz(r, r->_y);
}

static void op_eor(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	r->_a ^= value;
	updateFlagsNz(r, r->_a);
}

static void op_inc(CpuCtx *r, int mode, InstArg *arg) {
	r->_mem[arg->_ea._value]++;
	updateFlagsNz(r, r->_mem[arg->_ea._value]);
}

static void op_inx(CpuCtx *r, int mode, InstArg *arg) {
	r->_x++;
	updateFlagsNz(r, r->_x);
}

static void op_iny(CpuCtx *r, int mode, InstArg *arg) {
	r->_y++;
	updateFlagsNz(r, r->_y);
}

static void op_jmp(CpuCtx *r, int mode, InstArg *arg) {
	r->_pc = arg->_ea._value;
}

static void op_jsr(CpuCtx *r, int mode, InstArg *arg) {
	r->_pc--;
	r->_mem[0x100 + r->_sp--] = r->_pc >> 8;
	r->_mem[0x100 + r->_sp--] = r->_pc & 0xff;
	r->_pc = arg->_ea._value;
}

static void op_lda(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	r->_a = value;
	updateFlagsNz(r, r->_a);
}

static void op_ldx(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	r->_x = value;
	updateFlagsNz(r, r->_x);
}

static void op_ldy(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	r->_y = value;
	updateFlagsNz(r, r->_y);
}

static void op_lsr(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t *valuep = rmwTarget(r, mode, arg);
	updateCarry(r, *valuep & 1);
	*valuep >>= 1;
	updateFlagsNz(r, *valuep);
}

static void op_nop(CpuCtx *r, int mode, InstArg *arg) {}

static void op_ora(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	r->_a |= value;
	updateFlagsNz(r, r->_a);
}

static void op_pha(CpuCtx *r, int mode, InstArg *arg) {
	r->_mem[0x100 + r->_sp--] = r->_a;
}

static void op_php(CpuCtx *r, int mode, InstArg *arg) {
	r->_mem[0x100 + r->_sp--] = r->_flags & ~0x10;
}

static void op_pla(CpuCtx *r, int mode, InstArg *arg) {
	r->_a = r->_mem[0x100 + ++r->_sp];
	updateFlagsNz(r, r->_a);
}

static void op_plp(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags = r->_mem[0x100 + ++r->_sp];
}

static void op_rol(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t old_flags;
	uint8_t *valuep = rmwTarget(r, mode, arg);
	old_flags = r->_flags;
	updateCarry(r, *valuep & 128);
	*valuep <<= 1;
	*valuep |= (old_flags & FLAG_C) != 0 ? 1 : 0;
	updateFlagsNz(r, *valuep);
}

static void op_ror(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t old_flags;
	uint8_t *valuep = rmwTarget(r, mode, arg);
	old_flags = r->_flags;
	updateCarry(r, *valuep & 1);
	*valuep >>= 1;
	*valuep |= (old_flags & FLAG_C) != 0 ? 128 : 0;
	updateFlagsNz(r, *valuep);
}

static void op_rti(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags = r->_mem[0x100 + ++r->_sp];
	r->_pc = r->_mem[0x100 + ++r->_sp];
	r->_pc |= r->_mem[0x100 + ++r->_sp] << 8;
}

static void op_rts(CpuCtx *r, int mode, InstArg *arg) {
	r->_pc = r->_mem[0x100 + ++r->_sp];
	r->_pc |= r->_mem[0x100 + ++r->_sp] << 8;
	r->_pc++;
}

static void op_sbc(CpuCtx *r, int mode, InstArg *arg) {
	uint16_t result;
	uint8_t value = fetchOperand(r, mode, arg);
	result = subtract(r, r->_flags & FLAG_C, r->_a, value);
	update_overflow(r, !((r->_a & 0x80) ^ (value & 0x80)) && ((r->_a & 0x80) ^ (result & 0x80)));
	r->_a = result & 0xff;
	updateFlagsNz(r, r->_a);
}

static void op_sec(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags |= FLAG_C;
}

static void op_sed(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags |= FLAG_D;
}

static void op_sei(CpuCtx *r, int mode, InstArg *arg) {
	r->_flags |= FLAG_I;
}

static void op_sta(CpuCtx *r, int mode, InstArg *arg) {
	r->_mem[arg->_ea._value] = r->_a;
}

static void op_stx(CpuCtx *r, int mode, InstArg *arg) {
	r->_mem[arg->_ea._value] = r->_x;
}

static void op_sty(CpuCtx *r, int mode, InstArg *arg) {
	r->_mem[arg->_ea._value] = r->_y;
}

static void op_tax(CpuCtx *r, int mode, InstArg *arg) {
	r->_x = r->_a;
	updateFlagsNz(r, r->_x);
}

static void op_tay(CpuCtx *r, int mode, InstArg *arg) {
	r->_y = r->_a;
	updateFlagsNz(r, r->_y);
}

static void op_tsx(CpuCtx *r, int mode, InstArg *arg) {
	r->_x = r->_sp;
	updateFlagsNz(r, r->_x);
}

static void op_txa(CpuCtx *r, int mode, InstArg *arg) {
	r->_a = r->_x;
	updateFlagsNz(r, r->_a);
}

static void op_txs(CpuCtx *r, int mode, InstArg *arg) {
	r->_sp = r->_x;
}

static void op_tya(CpuCtx *r, int mode, InstArg *arg) {
	r->_a = r->_y;
	updateFlagsNz(r, r->_a);
}

/* iAN */
static void op_anc(CpuCtx *r, int mode, InstArg *arg) {
	uint8_t value = fetchOperand(r, mode, arg);
	r->_a &= value;
	if (r->_a & 0x80)
		r->_flags |= FLAG_C;
	updateFlagsNz(r, r->_a);
}

/* iAN */
static void op_lax(CpuCtx *r, int mode, InstArg *arg) {
	op_lda(r, mode, arg);
	r->_x = r->_a;
	updateFlagsNz(r, r->_x);
}
/* iAN */
static void op_lae(CpuCtx *r, int mode, InstArg *arg) {
	op_lda(r, mode, arg);
	r->_x = r->_a;
	r->_sp = r->_a;
	updateFlagsNz(r, r->_a);
}

/* iAN */
static void op_dcp(CpuCtx *r, int mode, InstArg *arg) {
	op_dec(r, mode, arg);
	op_cmp(r, mode, arg);
}

/* iAN */
/* Stores A & X; A and the flags are left alone. */
static void op_sax(CpuCtx *r, int mode, InstArg *arg) {
	r->_mem[arg->_ea._value] = r->_a & r->_x;
}

/* iAN */
static void op_rla(CpuCtx *r, int mode, InstArg *arg) {
	op_rol(r, mode, arg);
	op_and(r, mode, arg);
}

/* iAN */
static void op_rra(CpuCtx *r, int mode, InstArg *arg) {
	op_ror(r, mode, arg);
	op_adc(r, mode, arg);
}

/* iAN */
static void op_isb(CpuCtx *r, int mode, InstArg *arg) {
	op_inc(r, mode, arg);
	op_sbc(r, mode, arg);
}

/* iAN */
static void op_slo(CpuCtx *r, int mode, InstArg *arg) {
	op_asl(r, mode, arg);
	op_ora(r, mode, arg);
}

/* iAN */
static void op_sbx(CpuCtx *r, int mode, InstArg *arg) {
	/* immediate mode only: sbx #$ff */
	r->_x &= r->_a;
	/* SBX sets the carry like a CMP, but does not use it in the subtraction. */
	r->_flags = (r->_flags & 0xfe) | (r->_x >= arg->_imm._value ? 1 : 0);
	r->_x -= arg->_imm._value;
	updateFlagsNz(r, r->_x);
}

/* iAN */
static void op_sre(CpuCtx *r, int mode, InstArg *arg) {
	op_lsr(r, mode, arg);
	op_eor(r, mode, arg);
}

/* iAN */
/* First A AND #immediate, then LSR A. */
static void op_asr(CpuCtx *r, int mode, InstArg *arg) {
	op_and(r, MODE_IMMEDIATE, arg);
	op_lsr(r, MODE_ACCUMULATOR, arg);
}

/* iAN */
/* ANE and LXA are unstable: the chip ORs A with a "magic" value that depends
 * on the machine and its temperature before the AND. $ee is the value most
 * C64s show. */
static void op_ane(CpuCtx *r, int mode, InstArg *arg) {
	r->_a |= 0xee;
	r->_a &= r->_x;
	op_and(r, MODE_IMMEDIATE, arg);
}

/* iAN */
static void op_lxa(CpuCtx *r, int mode, InstArg *arg) {
	r->_a |= 0xee;
	op_and(r, MODE_IMMEDIATE, arg);
	r->_x = r->_a;
}

/* An opcode with no handler: the JAMs, and the unstable undocumented ones
 * that no depacker was seen to use ($6b ARR among them). nextInst reports
 * them as the end of the run. */
#define NULL_OP { nullptr, nullptr, 0 }

static const InstInfo g_ops[256] = {
	/* 0x00 */
	{ op_brk, mode_imp, 7 },
	{ op_ora, mode_indx, 6 },
	NULL_OP,
	NULL_OP,
	{ op_nop, mode_zp, 3 }, /* $04 nop $ff */
	{ op_ora, mode_zp, 3 },
	{ op_asl, mode_zp, 5 },
	{ op_slo, mode_zp, 5 }, /* $07 slo $ff */
	{ op_php, mode_imp, 3 },
	{ op_ora, mode_imm, 2 },
	{ op_asl, mode_acc, 2 },
	{ op_anc, mode_imm, 2 }, /* $0b anc #$ff */
	{ op_nop, mode_abs, 4 }, /* $0c nop $ffff */
	{ op_ora, mode_abs, 4 },
	{ op_asl, mode_abs, 6 },
	{ op_slo, mode_abs, 6 }, /* $0f slo $ffff */
	/* 0x10 */
	{ op_bpl, mode_rel, 2 },
	{ op_ora, mode_indy, 5 },
	NULL_OP,
	NULL_OP,
	{ op_nop, mode_zpx, 4 }, /* $14 nop $ff,x */
	{ op_ora, mode_zpx, 4 },
	{ op_asl, mode_zpx, 6 },
	NULL_OP,
	{ op_clc, mode_imp, 2 },
	{ op_ora, mode_absy, 4 },
	{ op_nop, mode_imp, 2 },  /* $1a nop */
	{ op_slo, mode_absy, 4 }, /* $1b slo $ffff,y */
	{ op_nop, mode_absx, 4 }, /* $1c nop $ffff,x */
	{ op_ora, mode_absx, 4 },
	{ op_asl, mode_absx, 7 },
	{ op_slo, mode_absx, 7 }, /* $1f slo $ffff,x */
	/* 0x20 */
	{ op_jsr, mode_abs, 6 },
	{ op_and, mode_indx, 6 },
	NULL_OP,
	{ op_rla, mode_indx, 8 }, /* $23 rla ($ff,x) */
	{ op_bit, mode_zp, 3 },
	{ op_and, mode_zp, 3 },
	{ op_rol, mode_zp, 5 },
	{ op_rla, mode_zp, 5 }, /* $27 rla $ff */
	{ op_plp, mode_imp, 4 },
	{ op_and, mode_imm, 2 },
	{ op_rol, mode_acc, 2 },
	{ op_anc, mode_imm, 2 }, /* $2b anc #$ff */
	{ op_bit, mode_abs, 4 },
	{ op_and, mode_abs, 4 },
	{ op_rol, mode_abs, 6 },
	{ op_rla, mode_abs, 6 }, /* $2f rla $ffff */
	/* 0x30 */
	{ op_bmi, mode_rel, 2 },
	{ op_and, mode_indy, 5 },
	NULL_OP,
	{ op_rla, mode_indy, 8 }, /* $33 rla ($ff),y  */
	{ op_nop, mode_zpx, 4 },  /* $34 nop $ff,x */
	{ op_and, mode_zpx, 4 },
	{ op_rol, mode_zpx, 6 },
	{ op_rla, mode_zpx, 6 }, /* $37 rla $ff,x */
	{ op_sec, mode_imp, 2 },
	{ op_and, mode_absy, 4 },
	{ op_nop, mode_imp, 2 },  /* $3a nop */
	{ op_rla, mode_absy, 7 }, /* $3b rla $ffff,y */
	{ op_nop, mode_absx, 7 }, /* $3c nop $ffff,x */
	{ op_and, mode_absx, 4 },
	{ op_rol, mode_absx, 7 },
	{ op_rla, mode_absx, 7 }, /* $3f rla $ffff,x */
	/* 0x40 */
	{ op_rti, mode_imp, 6 },
	{ op_eor, mode_indx, 6 },
	NULL_OP,
	{ op_sre, mode_indx, 6 }, /* $43 sre ($ff,x) */
	{ op_nop, mode_zp, 3 },   /* $44 nop $ff */
	{ op_eor, mode_zp, 3 },
	{ op_lsr, mode_zp, 5 },
	{ op_sre, mode_zp, 5 }, /* $47 sre $ff */
	{ op_pha, mode_imp, 3 },
	{ op_eor, mode_imm, 2 },
	{ op_lsr, mode_acc, 2 },
	{ op_asr, mode_imm, 2 }, /* $4b asr #$ff */
	{ op_jmp, mode_abs, 3 },
	{ op_eor, mode_abs, 4 },
	{ op_lsr, mode_abs, 6 },
	{ op_sre, mode_abs, 6 }, /* $4f sre $ffff */
	/* 0x50 */
	{ op_bvc, mode_rel, 2 },
	{ op_eor, mode_indy, 5 },
	NULL_OP,
	NULL_OP,
	{ op_nop, mode_zpx, 4 }, /* $54 nop $ff,x */
	{ op_eor, mode_zpx, 4 }, /* fix: eor $ff,x takes 4 cycles*/
	{ op_lsr, mode_zpx, 6 },
	{ op_sre, mode_zpx, 6 }, /* $57 sre $ff,x */
	{ op_cli, mode_imp, 2 },
	{ op_eor, mode_absy, 4 },
	{ op_nop, mode_imp, 2 },  /* $5a nop */
	{ op_sre, mode_absy, 7 }, /* $5b sre $ffff,y */
	{ op_nop, mode_absx, 4 }, /* $5c nop $ffff,x */
	{ op_eor, mode_absx, 4 },
	{ op_lsr, mode_absx, 7 },
	{ op_sre, mode_absx, 7 }, /* $5f sre $ffff,x */
	/* 0x60 */
	{ op_rts, mode_imp, 6 },
	{ op_adc, mode_indx, 6 },
	NULL_OP,
	{ op_rra, mode_indx, 8 }, /* $63 rra ($ff,x) */
	{ op_nop, mode_zp, 3 },   /* $64 nop $ff */
	{ op_adc, mode_zp, 3 },
	{ op_ror, mode_zp, 5 },
	{ op_rra, mode_zp, 5 }, /* $67 rra $ff */
	{ op_pla, mode_imp, 4 },
	{ op_adc, mode_imm, 2 },
	{ op_ror, mode_acc, 2 },
	NULL_OP,                     /* fix: $6b ARR (todo?) */
	{ op_jmp, mode_ind, 5 }, /* fix: $6c JMP ($FFFF) */
	{ op_adc, mode_abs, 4 },
	{ op_ror, mode_abs, 6 },
	{ op_rra, mode_abs, 6 }, /* $6f rra $ffff */
	/* 0x70 */
	{ op_bvs, mode_rel, 2 },
	{ op_adc, mode_indy, 5 },
	NULL_OP,
	{ op_rra, mode_indy, 8 }, /* $73 rra ($ff),y */
	{ op_nop, mode_zpx, 4 },  /* $74 nop $ff,x */
	{ op_adc, mode_zpx, 4 },
	{ op_ror, mode_zpx, 6 },
	{ op_rra, mode_zpx, 6 }, /* $77 rra $ff,x */
	{ op_sei, mode_imp, 2 },
	{ op_adc, mode_absy, 4 },
	{ op_nop, mode_imp, 2 },  /* $7a nop */
	{ op_rra, mode_absy, 7 }, /* $7b rra $ffff,y */
	{ op_nop, mode_absx, 4 }, /* $7c nop $ffff,x */
	{ op_adc, mode_absx, 4 },
	{ op_ror, mode_absx, 7 },
	{ op_rra, mode_absx, 7 }, /* $7f rra $ffff,x */
	/* 0x80 */
	{ op_nop, mode_imm, 2 }, /* $80 nop #$ff */
	{ op_sta, mode_indx, 6 },
	{ op_nop, mode_imm, 2 },  /* $82 nop #$ff */
	{ op_sax, mode_indx, 6 }, /* $83 sax ($ff,x) */
	{ op_sty, mode_zp, 3 },
	{ op_sta, mode_zp, 3 },
	{ op_stx, mode_zp, 3 },
	{ op_sax, mode_zp, 3 }, /* $87 sax $ff */
	{ op_dey, mode_imp, 2 },
	{ op_nop, mode_imm, 2 }, /* $89 nop #$ff */
	{ op_txa, mode_imp, 2 },
	{ op_ane, mode_imm, 2 }, /* $8b ane #$ff */
	{ op_sty, mode_abs, 4 },
	{ op_sta, mode_abs, 4 },
	{ op_stx, mode_abs, 4 },
	{ op_sax, mode_abs, 4 }, /* $8f sax $ffff */
	/* 0x90 */
	{ op_bcc, mode_rel, 2 },
	{ op_sta, mode_indy, 6 },
	NULL_OP,
	NULL_OP,
	{ op_sty, mode_zpx, 4 },
	{ op_sta, mode_zpx, 4 },
	{ op_stx, mode_zpy, 4 },
	{ op_sax, mode_zpy, 4 }, /* $97 sax $ff,y */
	{ op_tya, mode_imp, 2 },
	{ op_sta, mode_absy, 5 },
	{ op_txs, mode_imp, 2 },
	NULL_OP,
	NULL_OP,
	{ op_sta, mode_absx, 5 },
	NULL_OP,
	NULL_OP,
	/* 0xa0 */
	{ op_ldy, mode_imm, 2 },
	{ op_lda, mode_indx, 6 },
	{ op_ldx, mode_imm, 2 },
	{ op_lax, mode_indx, 6 }, /* $a3 lax ($ff,x) */
	{ op_ldy, mode_zp, 3 },
	{ op_lda, mode_zp, 3 },
	{ op_ldx, mode_zp, 3 },
	{ op_lax, mode_zp, 3 }, /* $a7 lax $ff */
	{ op_tay, mode_imp, 2 },
	{ op_lda, mode_imm, 2 },
	{ op_tax, mode_imp, 2 },
	{ op_lxa, mode_imm, 2 }, /* $ab lxa #$ff */
	{ op_ldy, mode_abs, 4 },
	{ op_lda, mode_abs, 4 },
	{ op_ldx, mode_abs, 4 },
	{ op_lax, mode_abs, 4 }, /* $af lax $ffff */
	/* 0xb0 */
	{ op_bcs, mode_rel, 2 },
	{ op_lda, mode_indy, 5 },
	NULL_OP,
	{ op_lax, mode_indy, 5 }, /* $b3 lax ($ff),y */
	{ op_ldy, mode_zpx, 4 },
	{ op_lda, mode_zpx, 4 },
	{ op_ldx, mode_zpy, 4 },
	{ op_lax, mode_zpy, 4 }, /* $b7 lax $ff,y */
	{ op_clv, mode_imp, 2 },
	{ op_lda, mode_absy, 4 },
	{ op_tsx, mode_imp, 2 },
	{ op_lae, mode_absy, 4 }, /* $bb lae $ffff,y */
	{ op_ldy, mode_absx, 4 },
	{ op_lda, mode_absx, 4 },
	{ op_ldx, mode_absy, 4 },
	{ op_lax, mode_absy, 4 }, /* $bf lax $ffff,y */
	/* 0xc0 */
	{ op_cpy, mode_imm, 2 },
	{ op_cmp, mode_indx, 6 },
	{ op_nop, mode_imm, 2 },  /* $c2 nop #$ff */
	{ op_dcp, mode_indx, 8 }, /* $c3 dcp ($ff,x) */
	{ op_cpy, mode_zp, 3 },
	{ op_cmp, mode_zp, 3 },
	{ op_dec, mode_zp, 5 },
	{ op_dcp, mode_zp, 5 }, /* $c7 dcp $FF */
	{ op_iny, mode_imp, 2 },
	{ op_cmp, mode_imm, 2 },
	{ op_dex, mode_imp, 2 },
	{ op_sbx, mode_imm, 2 }, /* $cb sbx #$ff */
	{ op_cpy, mode_abs, 4 },
	{ op_cmp, mode_abs, 4 },
	{ op_dec, mode_abs, 6 },
	{ op_dcp, mode_abs, 6 }, /* $cf dcp $ffff */
	/* 0xd0 */
	{ op_bne, mode_rel, 2 },
	{ op_cmp, mode_indy, 5 },
	NULL_OP,
	{ op_dcp, mode_indy, 8 }, /* $d3 dcp ($ff),y */
	{ op_nop, mode_zpx, 4 },  /* $d4 nop $ff,x */
	{ op_cmp, mode_zpx, 4 },
	{ op_dec, mode_zpx, 6 },
	{ op_dcp, mode_zpx, 6 }, /* $d7 dcp $ff,x */
	{ op_cld, mode_imp, 2 },
	{ op_cmp, mode_absy, 4 },
	{ op_nop, mode_imp, 2 },  /* $da nop */
	{ op_dcp, mode_absy, 7 }, /* $db dcp $ffff,y */
	{ op_nop, mode_absx, 4 }, /* $dc nop $ffff,x */
	{ op_cmp, mode_absx, 4 },
	{ op_dec, mode_absx, 7 },
	{ op_dcp, mode_absx, 7 }, /* $df dcp $ffff,x */
	/* 0xe0 */
	{ op_cpx, mode_imm, 2 },
	{ op_sbc, mode_indx, 6 },
	{ op_nop, mode_imm, 2 },  /* $e2 nop #$ff */
	{ op_isb, mode_indx, 8 }, /* $e3 isb ($ff,x) */
	{ op_cpx, mode_zp, 3 },
	{ op_sbc, mode_zp, 3 },
	{ op_inc, mode_zp, 5 },
	{ op_isb, mode_zp, 5 }, /* $e7 isb $ff */
	{ op_inx, mode_imp, 2 },
	{ op_sbc, mode_imm, 2 },
	{ op_nop, mode_imp, 2 },
	{ op_sbc, mode_imm, 2 }, /* $eb sbc #$ff */
	{ op_cpx, mode_abs, 4 },
	{ op_sbc, mode_abs, 4 },
	{ op_inc, mode_abs, 6 },
	{ op_isb, mode_abs, 6 }, /* $ef isb $ffff */
	/* 0xf0 */
	{ op_beq, mode_rel, 2 },
	{ op_sbc, mode_indy, 5 },
	NULL_OP,
	{ op_isb, mode_indy, 8 }, /* $f3 isb ($ff),y */
	{ op_nop, mode_zpx, 4 },  /* $f4 nop $ff,x */
	{ op_sbc, mode_zpx, 4 },
	{ op_inc, mode_zpx, 6 },
	{ op_isb, mode_zpx, 6 }, /* $f7 isb $ff,x */
	{ op_sed, mode_imp, 2 },
	{ op_sbc, mode_absy, 4 },
	{ op_nop, mode_imp, 2 },  /* $fa nop */
	{ op_isb, mode_absy, 7 }, /* $fb isb $ffff,y */
	{ op_nop, mode_absx, 7 }, /* $fc nop $ffff,x */
	{ op_sbc, mode_absx, 4 },
	{ op_inc, mode_absx, 7 },
	{ op_isb, mode_absx, 7 }, /* $ff isb $ffff,x */
};

/* The fire button and the space bar are pressed on every other read, which
 * gets past both "wait for a press" and "wait for the release". */
static int flipfire(CpuCtx *r) {
	r->_retfire ^= 0x90;
	return r->_retfire;
}

static int flipspace(CpuCtx *r) {
	r->_retspace ^= 0x10;
	return r->_retspace;
}

/* True if `opCode` is a 6502 instruction that writes to memory at an
 * absolute address (so reaching the I/O page would have a side effect
 * we'd need to suppress rather than let pass through to RAM). */
static bool isAbsoluteWriteOpcode(int opCode) {
	switch (opCode) {
	case 0x0E: case 0x1E:           /* ASL $ffff[,X] */
	case 0xCE: case 0xDE:           /* DEC $ffff[,X] */
	case 0xEE: case 0xFE:           /* INC $ffff[,X] */
	case 0x4E: case 0x5E:           /* LSR $ffff[,X] */
	case 0x2E: case 0x3E:           /* ROL $ffff[,X] */
	case 0x6E: case 0x7E:           /* ROR $ffff[,X] */
	case 0x8D: case 0x9D: case 0x99: /* STA $ffff / $ffff,X / $ffff,Y */
	case 0x91:                      /* STA ($ff),Y */
	case 0x8E:                      /* STX $ffff */
	case 0x8C:                      /* STY $ffff */
		return true;
	default:
		return false;
	}
}

/* Handle a write to the I/O page ($D000-$DFFF). The actual store is
 * skipped (we don't model the VIC/SID/CIA registers); we just charge
 * cycles and shadow $D011 so subsequent reads see the value back. */
static void interceptIOWrite(CpuCtx *r, int opCode, const InstArg *arg) {
	r->_cycles += g_ops[opCode]._cycles;
	if (arg->_ea._value == 0xd011) {
		switch (opCode) {
		case 0x8D: r->_d011[0] = r->_a & 0x7f; break;
		case 0x8E: r->_d011[0] = r->_x & 0x7f; break;
		case 0x8C: r->_d011[0] = r->_y & 0x7f; break;
		default: break;
		}
	}
}

/* Handle a read from the I/O page. Returns true if the opcode was one
 * of the few we synthesize (raster register, CIA1 joystick/keyboard,
 * CIA2 idle), false if the read should fall through to normal dispatch.
 *
 * We update the synthetic raster shadow first so consumers see a value
 * that advances with cycle count — that's what breaks `wait for raster`
 * loops in packer intros. */
static bool interceptIORead(CpuCtx *r, int opCode, const InstArg *arg) {
	r->_d011[1] = (r->_cycles / 0x3f) % 0x157;
	r->_d011[0] = (r->_d011[0] & 0x7f) | ((r->_d011[1] & 0x100) >> 1);
	r->_d011[1] &= 0xff;

	const int ea = arg->_ea._value;
	const bool isCia1Port = (ea == 0xdc00 || ea == 0xdc01);
	const bool isRaster   = (ea == 0xd011 || ea == 0xd012);
	const int cycles = g_ops[opCode]._cycles;

	switch (opCode) {
	/* LDA / LAX absolute. */
	case 0xad:
	case 0xaf:
		if (isRaster) {
			r->_cycles += cycles;
			r->_a = r->_d011[ea - 0xd011];
			if (opCode == 0xaf) r->_x = r->_a;
			updateFlagsNz(r, r->_a);
			return true;
		}
		if (isCia1Port) {
			r->_cycles += cycles;
			r->_a = (ea == 0xdc00) ? flipfire(r) : flipspace(r);
			if (opCode == 0xaf) r->_x = r->_a;
			updateFlagsNz(r, r->_a);
			return true;
		}
		if (ea >= 0xdd01 && ea <= 0xdd0f) {
			r->_cycles += cycles;
			r->_a = 0xff;
			if (opCode == 0xaf) r->_x = r->_a;
			updateFlagsNz(r, r->_a);
			return true;
		}
		return false;

	/* AND absolute against joystick/keyboard port. */
	case 0x2d:
		if (isCia1Port) {
			r->_cycles += cycles;
			r->_a &= (ea == 0xdc00) ? flipfire(r) : flipspace(r);
			updateFlagsNz(r, r->_a);
			return true;
		}
		return false;

	/* LDX absolute. */
	case 0xae:
		if (isRaster) {
			r->_cycles += cycles;
			r->_x = r->_d011[ea - 0xd011];
			updateFlagsNz(r, r->_x);
			return true;
		}
		if (isCia1Port) {
			r->_cycles += cycles;
			r->_x = (ea == 0xdc00) ? flipfire(r) : flipspace(r);
			updateFlagsNz(r, r->_x);
			return true;
		}
		return false;

	/* LDY absolute. */
	case 0xac:
		if (isRaster) {
			r->_cycles += cycles;
			r->_y = r->_d011[ea - 0xd011];
			updateFlagsNz(r, r->_y);
			return true;
		}
		if (isCia1Port) {
			r->_cycles += cycles;
			r->_y = (ea == 0xdc00) ? flipfire(r) : flipspace(r);
			updateFlagsNz(r, r->_y);
			return true;
		}
		return false;

	/* BIT against the VIC raster — set N/V from the shadow byte, Z from (byte & A). */
	case 0x2c:
		if (isRaster) {
			r->_cycles += cycles;
			int bt = r->_d011[ea - 0xd011];
			r->_flags &= ~(FLAG_N | FLAG_V | FLAG_Z);
			r->_flags |= (bt & FLAG_N) != 0 ? FLAG_N : 0;
			r->_flags |= (bt & FLAG_V) != 0 ? FLAG_V : 0;
			r->_flags |= (bt & r->_a) == 0 ? FLAG_Z : 0;
			return true;
		}
		return false;

	/* CMP / CPX / CPY against raster or joystick port. */
	case 0xcd:
	case 0xec:
	case 0xcc: {
		auto regForCompare = [&]() {
			if (opCode == 0xec) return r->_x;
			if (opCode == 0xcc) return r->_y;
			return r->_a;
		};
		if (isRaster) {
			r->_cycles += cycles;
			subtract(r, 1, regForCompare(), r->_d011[ea - 0xd011]);
			return true;
		}
		if (isCia1Port) {
			r->_cycles += cycles;
			int bt = (ea == 0xdc00) ? flipfire(r) : flipspace(r);
			subtract(r, 1, regForCompare(), bt);
			return true;
		}
		return false;
	}

	default:
		return false;
	}
}

#ifdef UNP64_TRACE
/* Test builds only (test/Makefile, `make trace`): called with the CPU state
 * in front of every instruction, so that two builds can be compared step by
 * step and not just by what they unpack to. */
void unp64TraceHook(const CpuCtx *r);
#endif

/* Step the 6502 CPU one instruction.
 *
 * Returns false to continue emulation, true to stop — either because the
 * opcode is unimplemented (no handler in g_ops) or because we just
 * decoded BRK ($00), which depackers should never legitimately reach.
 *
 * When the I/O page ($D000-$DFFF) is banked in, we don't emulate the
 * VIC/SID/CIA chips themselves — instead we intercept the common
 * opcodes that touch I/O and either suppress side effects (writes) or
 * return plausible synthesized values (raster line, joystick/keyboard). */
bool nextInst(CpuCtx *r) {
#ifdef UNP64_TRACE
	unp64TraceHook(r);
#endif
	/* Zeroed: the implied, accumulator, immediate and relative modes fill
	 * at most one byte of the union, and the I/O-page test below reads
	 * _ea._value whatever the mode. */
	InstArg arg[1] = {};
	int opCode = r->_mem[r->_pc];
	const InstInfo *info = g_ops + opCode;
	if (info->_op == nullptr) {
		return true; /* unimplemented / illegal opcode */
	}
	int mode = info->_mode(r, arg);

	/* I/O page is visible when CPU port bits 0-2 are 5, 6 or 7. */
	const bool ioVisible = (r->_mem[1] & 0x7) >= 5;
	const bool targetsIO = arg->_ea._value >= 0xd000 && arg->_ea._value < 0xe000;

	if (ioVisible && targetsIO) {
		if (isAbsoluteWriteOpcode(opCode)) {
			interceptIOWrite(r, opCode, arg);
			return false;
		}
		if (interceptIORead(r, opCode, arg)) {
			return false;
		}
		/* Fall through: I/O read we don't model — let normal dispatch run. */
	}

	info->_op(r, mode, arg);
	r->_cycles += info->_cycles;
	return opCode == 0; /* BRK: depacker off the rails, stop. */
}

} // End of namespace Unp64
