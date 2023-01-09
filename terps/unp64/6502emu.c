/*
Original: Magnus Lind
Modifications for UNP64: iAN CooG
*/
#include "6502emu.h"
#include "log.h"
#include <stdlib.h>

#define FLAG_N 128
#define FLAG_V 64
#define FLAG_D 8
#define FLAG_I 4
#define FLAG_Z 2
#define FLAG_C 1

extern int debugprint;

struct arg_ea {
    u16 value;
};
struct arg_relative {
    i8 value;
};
struct arg_immediate {
    u8 value;
};

union inst_arg {
    struct arg_ea ea;
    struct arg_relative rel;
    struct arg_immediate imm;
};

typedef void op_f(struct cpu_ctx *r, int mode, union inst_arg *arg);
typedef int mode_f(struct cpu_ctx *r, union inst_arg *arg);

struct op_info {
    op_f *f;
    char *fmt;
};

struct mode_info {
    mode_f *f;
    char *fmt;
};

struct inst_info {
    struct op_info *op;
    struct mode_info *mode;
    u8 cycles;
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

static int mode_imm(struct cpu_ctx *r, union inst_arg *arg)
{
    arg->imm.value = r->mem[r->pc + 1];
    r->pc += 2;
    return MODE_IMMEDIATE;
}
static int mode_zp(struct cpu_ctx *r, union inst_arg *arg)
{
    arg->ea.value = r->mem[r->pc + 1];
    r->pc += 2;
    return MODE_ZERO_PAGE;
}
static int mode_zpx(struct cpu_ctx *r,
    union inst_arg *arg)
{ /* iAN: ldx #1 lda $ff,x should fetch
                                              from $00 and not $100 */
    u8 lsbLo = (r->mem[r->pc + 1] + r->x) & 0xff;
    arg->ea.value = lsbLo;
    r->pc += 2;
    return MODE_ZERO_PAGE_X;
}
static int mode_zpy(struct cpu_ctx *r,
    union inst_arg *arg)
{ /* iAN: ldy #1 ldx $ff,y should fetch
                                              from $00 and not $100 */
    u8 lsbLo = (r->mem[r->pc + 1] + r->y) & 0xff;
    arg->ea.value = lsbLo;
    r->pc += 2;
    return MODE_ZERO_PAGE_Y;
}
static int mode_abs(struct cpu_ctx *r, union inst_arg *arg)
{
    u16 offset = r->mem[r->pc + 1];
    u16 base = r->mem[r->pc + 2] << 8;
    arg->ea.value = base + offset;
    r->pc += 3;
    return MODE_ABSOLUTE;
}
static int mode_absx(struct cpu_ctx *r, union inst_arg *arg)
{
    u16 offset = r->mem[r->pc + 1] + r->x;
    u16 base = r->mem[r->pc + 2] << 8;
    arg->ea.value = base + offset;
    r->pc += 3;
    r->cycles += (offset > 255);
    return MODE_ABSOLUTE_X;
}
static int mode_absy(struct cpu_ctx *r, union inst_arg *arg)
{
    u16 offset = r->mem[r->pc + 1] + r->y;
    u16 base = r->mem[r->pc + 2] << 8;
    arg->ea.value = base + offset;
    r->pc += 3;
    r->cycles += (offset > 255);
    return MODE_ABSOLUTE_Y;
}
static int mode_ind(struct cpu_ctx *r, union inst_arg *arg)
{
    /* iAN: Wrong calcs, for example I have a jmp ($0512), at $0512 there's
     $0b $08 $00 $ff, this returns $0800 instead of $80b
     Also, the wraparound bug is not handled as it should:
     JMP ($12FF) should fetch lobyte from $12FF & hibyte from $1200
     */

    /* wrong
     u8 lsbLo = r->mem[r->pc + 1];
     u8 msbLo = lsbLo + 1;
     u16 base = r->mem[msbLo + (r->mem[r->pc + 2] << 8)] << 8;
     u16 offset = r->mem[lsbLo];
     arg->ea.value = base + offset;
     */

    /* correction */
    int lsbLo = r->mem[r->pc + 1];
    int msbLo = r->mem[r->pc + 2];
    int offsetlo = lsbLo | msbLo << 8;
    int offsethi = ((lsbLo + 1) & 0xff) | msbLo << 8;
    arg->ea.value = r->mem[offsetlo] | r->mem[offsethi] << 8;

    r->pc += 3;
    return MODE_INDIRECT;
}
static int mode_indx(struct cpu_ctx *r, union inst_arg *arg)
{
    u8 lsbLo = r->mem[r->pc + 1] + r->x;
    u8 msbLo = lsbLo + 1;
    u16 base = r->mem[msbLo] << 8;
    u16 offset = r->mem[lsbLo];
    arg->ea.value = base + offset;
    r->pc += 2;
    return MODE_INDIRECT_X;
}
static int mode_indy(struct cpu_ctx *r, union inst_arg *arg)
{
    u8 lsbLo = r->mem[r->pc + 1];
    u8 msbLo = lsbLo + 1;
    u16 base = r->mem[msbLo] << 8;
    u16 offset = r->mem[lsbLo] + r->y;
    arg->ea.value = base + offset;
    r->pc += 2;
    r->cycles += (offset > 255);
    return MODE_INDIRECT_Y;
}
static int mode_rel(struct cpu_ctx *r, union inst_arg *arg)
{
    arg->rel.value = r->mem[r->pc + 1];
    r->pc += 2;
    return MODE_RELATIVE;
}
static int mode_acc(struct cpu_ctx *r, union inst_arg *arg)
{
    r->pc++;
    return MODE_ACCUMULATOR;
}
static int mode_imp(struct cpu_ctx *r, union inst_arg *arg)
{
    r->pc++;
    return MODE_IMPLIED;
}

static struct mode_info mode_imm_o = { &mode_imm, "#$%02x" };
static struct mode_info mode_zp_o = { &mode_zp, "$%02x" };
static struct mode_info mode_zpx_o = { &mode_zpx, "$%02x,x" };
static struct mode_info mode_zpy_o = { &mode_zpy, "$%02x,y" };
static struct mode_info mode_abs_o = { &mode_abs, "$%04x" };
static struct mode_info mode_absx_o = { &mode_absx, "$%04x,x" };
static struct mode_info mode_absy_o = { &mode_absy, "$%04x,y" };
static struct mode_info mode_ind_o = { &mode_ind, "($%04x)" };
static struct mode_info mode_indx_o = { &mode_indx, "($%02x,x)" };
static struct mode_info mode_indy_o = { &mode_indy, "($%02x),y" };
static struct mode_info mode_rel_o = { &mode_rel, "$%02x" };
static struct mode_info mode_acc_o = { &mode_acc, "a" };
static struct mode_info mode_imp_o = { &mode_imp, NULL };

static void update_flags_nz(struct cpu_ctx *r, u8 value)
{
    r->flags &= ~(FLAG_Z | FLAG_N);
    r->flags |= (value == 0 ? FLAG_Z : 0) | (value & FLAG_N);
}

static void update_carry(struct cpu_ctx *r, int bool)
{
    r->flags = (r->flags & ~FLAG_C) | (bool != 0 ? FLAG_C : 0);
}

static void update_overflow(struct cpu_ctx *r, int bool)
{
    r->flags = (r->flags & ~FLAG_V) | (bool != 0 ? FLAG_V : 0);
}

static void op_adc(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    u16 result;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    result = r->a + value + (r->flags & FLAG_C);
    update_carry(r, result & 256);
    update_overflow(r, !((r->a & 0x80) ^ (value & 0x80)) && ((r->a & 0x80) ^ (result & 0x80)));
    r->a = result & 0xff;
    update_flags_nz(r, r->a);
}

static void op_and(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    r->a &= value;
    update_flags_nz(r, r->a);
}

static void op_asl(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 *valuep;
    switch (mode) {
    case MODE_ACCUMULATOR:
        valuep = &r->a;
        break;
    default:
        valuep = &r->mem[arg->ea.value];
        break;
    }
    update_carry(r, *valuep & 128);
    *valuep <<= 1;
    update_flags_nz(r, *valuep);
}

static void branch(struct cpu_ctx *r, union inst_arg *arg)
{
    u16 target = r->pc + arg->rel.value;
    r->cycles += 1 + ((target & ~255) != (r->pc & ~255));
    r->pc = target;
}

static void op_bcc(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    if (!(r->flags & FLAG_C)) {
        branch(r, arg);
    }
}

static void op_bcs(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    if (r->flags & FLAG_C) {
        branch(r, arg);
    }
}

static void op_beq(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    if (r->flags & FLAG_Z) {
        branch(r, arg);
    }
}

static void op_bit(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags &= ~(FLAG_N | FLAG_V | FLAG_Z);
    r->flags |= (r->mem[arg->ea.value] & FLAG_N) != 0 ? FLAG_N : 0;
    r->flags |= (r->mem[arg->ea.value] & FLAG_V) != 0 ? FLAG_V : 0;
    r->flags |= (r->mem[arg->ea.value] & r->a) == 0 ? FLAG_Z : 0;
}

static void op_bmi(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    if (r->flags & FLAG_N) {
        branch(r, arg);
    }
}

static void op_bne(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    if (!(r->flags & FLAG_Z)) {
        branch(r, arg);
    }
}

static void op_bpl(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    if (!(r->flags & FLAG_N)) {
        branch(r, arg);
    }
}

static void op_brk(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->mem[0x100 + r->sp--] = (r->pc + 1) >> 8;
    r->mem[0x100 + r->sp--] = r->pc + 1;
    r->mem[0x100 + r->sp--] = r->flags | 0x10;
}

static void op_bvc(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    if (!(r->flags & FLAG_V)) {
        branch(r, arg);
    }
}

static void op_bvs(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    if ((r->flags & FLAG_V)) {
        branch(r, arg);
    }
}

static void op_clc(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags &= ~FLAG_C;
}

static void op_cld(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags &= ~FLAG_D;
}

static void op_cli(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags &= ~FLAG_I;
}

static void op_clv(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags &= ~FLAG_V;
}

static u16 subtract(struct cpu_ctx *r, int carry, u8 val1, u8 value)
{
    u16 target = val1 - value - (1 - !!carry);
    update_carry(r, !(target & 256));
    update_flags_nz(r, target & 255);
    return target;
}

static void op_cmp(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    subtract(r, 1, r->a, value);
}

static void op_cpx(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    subtract(r, 1, r->x, value);
}

static void op_cpy(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    subtract(r, 1, r->y, value);
}

static void op_dec(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->mem[arg->ea.value]--;
    update_flags_nz(r, r->mem[arg->ea.value]);
}

static void op_dex(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->x--;
    update_flags_nz(r, r->x);
}

static void op_dey(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->y--;
    update_flags_nz(r, r->y);
}

static void op_eor(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    r->a ^= value;
    update_flags_nz(r, r->a);
}

static void op_inc(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->mem[arg->ea.value]++;
    update_flags_nz(r, r->mem[arg->ea.value]);
}

static void op_inx(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->x++;
    update_flags_nz(r, r->x);
}

static void op_iny(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->y++;
    update_flags_nz(r, r->y);
}

static void op_jmp(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->pc = arg->ea.value;
}

static void op_jsr(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->pc--;
    r->mem[0x100 + r->sp--] = r->pc >> 8;
    r->mem[0x100 + r->sp--] = r->pc & 0xff;
    r->pc = arg->ea.value;
}

static void op_lda(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    r->a = value;
    update_flags_nz(r, r->a);
}

static void op_ldx(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    r->x = value;
    update_flags_nz(r, r->x);
}

static void op_ldy(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    r->y = value;
    update_flags_nz(r, r->y);
}

static void op_lsr(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 *valuep;
    switch (mode) {
    case MODE_ACCUMULATOR:
        valuep = &r->a;
        break;
    default:
        valuep = &r->mem[arg->ea.value];
        break;
    }
    update_carry(r, *valuep & 1);
    *valuep >>= 1;
    update_flags_nz(r, *valuep);
}

static void op_nop(struct cpu_ctx *r, int mode, union inst_arg *arg) { }

static void op_ora(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    r->a |= value;
    update_flags_nz(r, r->a);
}

static void op_pha(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->mem[0x100 + r->sp--] = r->a;
}

static void op_php(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->mem[0x100 + r->sp--] = r->flags & ~0x10;
}

static void op_pla(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->a = r->mem[0x100 + ++r->sp];
    update_flags_nz(r, r->a);
}

static void op_plp(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags = r->mem[0x100 + ++r->sp];
}

static void op_rol(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 *valuep;
    u8 old_flags;
    switch (mode) {
    case MODE_ACCUMULATOR:
        valuep = &r->a;
        break;
    default:
        valuep = &r->mem[arg->ea.value];
        break;
    }
    old_flags = r->flags;
    update_carry(r, *valuep & 128);
    *valuep <<= 1;
    *valuep |= (old_flags & FLAG_C) != 0 ? 1 : 0;
    update_flags_nz(r, *valuep);
}

static void op_ror(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 *valuep;
    u8 old_flags;
    switch (mode) {
    case MODE_ACCUMULATOR:
        valuep = &r->a;
        break;
    default:
        valuep = &r->mem[arg->ea.value];
        break;
    }
    old_flags = r->flags;
    update_carry(r, *valuep & 1);
    *valuep >>= 1;
    *valuep |= (old_flags & FLAG_C) != 0 ? 128 : 0;
    update_flags_nz(r, *valuep);
}

static void op_rti(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags = r->mem[0x100 + ++r->sp];
    r->pc = r->mem[0x100 + ++r->sp];
    r->pc |= r->mem[0x100 + ++r->sp] << 8;
}

static void op_rts(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->pc = r->mem[0x100 + ++r->sp];
    r->pc |= r->mem[0x100 + ++r->sp] << 8;
    r->pc++;
}

static void op_sbc(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    u16 result;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    result = subtract(r, r->flags & FLAG_C, r->a, value);
    update_overflow(r, !((r->a & 0x80) ^ (value & 0x80)) && ((r->a & 0x80) ^ (result & 0x80)));
    r->a = result & 0xff;
    update_flags_nz(r, r->a);
}

static void op_sec(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags |= FLAG_C;
}

static void op_sed(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags |= FLAG_D;
}

static void op_sei(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->flags |= FLAG_I;
}

static void op_sta(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->mem[arg->ea.value] = r->a;
}

static void op_stx(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->mem[arg->ea.value] = r->x;
}

static void op_sty(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->mem[arg->ea.value] = r->y;
}

static void op_tax(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->x = r->a;
    update_flags_nz(r, r->x);
}

static void op_tay(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->y = r->a;
    update_flags_nz(r, r->y);
}

static void op_tsx(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->x = r->sp;
    update_flags_nz(r, r->x);
}

static void op_txa(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->a = r->x;
    update_flags_nz(r, r->a);
}

static void op_txs(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->sp = r->x;
}

static void op_tya(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->a = r->y;
    update_flags_nz(r, r->a);
}

/* iAN */
static void op_anc(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    u8 value;
    switch (mode) {
    case MODE_IMMEDIATE:
        value = arg->imm.value;
        break;
    default:
        value = r->mem[arg->ea.value];
        break;
    }
    r->a &= value;
    if (r->a & 0x80)
        r->flags |= FLAG_C;
    update_flags_nz(r, r->a);
}

/* iAN */
static void op_lax(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    op_lda(r, mode, arg);
    r->x = r->a;
    update_flags_nz(r, r->x);
}
/* iAN */
static void op_lae(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    op_lda(r, mode, arg);
    r->x = r->a;
    r->sp = r->a;
    update_flags_nz(r, r->a);
}

/* iAN */
static void op_dcp(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    op_dec(r, mode, arg);
    op_cmp(r, mode, arg);
}

/* iAN */
static void op_sax(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->a &= r->x;
    op_sta(r, mode, arg);
}

/* iAN */
static void op_rla(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    op_rol(r, mode, arg);
    op_and(r, mode, arg);
}

/* iAN */
static void op_rra(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    op_rol(r, mode, arg);
    op_adc(r, mode, arg);
}

/* iAN */
static void op_isb(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    op_inc(r, mode, arg);
    op_sbc(r, mode, arg);
}

/* iAN */
static void op_slo(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    op_asl(r, mode, arg);
    op_ora(r, mode, arg);
}

/* iAN */
static void op_sbx(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    /* immediate mode only: sbx #$ff */
    r->x &= r->a;
    r->flags = (r->flags & 0xfe) | (r->x >= arg->imm.value ? 1 : 0); /* fixed: Carry IS set by SBX but
                                                  not used during subtraction */
    r->x -= arg->imm.value;
    update_flags_nz(r, r->x);
}

/* iAN */
static void op_sre(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    op_lsr(r, mode, arg);
    op_eor(r, mode, arg);
}
/* iAN */
static void
op_asr(struct cpu_ctx *r, int mode,
    union inst_arg *arg)
{ /* first A AND #immediate, then LSR A */
    op_and(r, MODE_IMMEDIATE, arg);
    op_lsr(r, MODE_ACCUMULATOR, arg);
}
/* iAN */
static void op_ane(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->a |= 0xee;
    r->a &= r->x;
    op_and(r, MODE_IMMEDIATE, arg);
}
/* iAN */
static void op_lxa(struct cpu_ctx *r, int mode, union inst_arg *arg)
{
    r->a |= 0xee;
    op_and(r, MODE_IMMEDIATE, arg);
    r->x = r->a;
}

static struct op_info op_adc_o = { &op_adc, "adc" };
static struct op_info op_anc_o = { &op_anc, "anc" };
static struct op_info op_and_o = { &op_and, "and" };
static struct op_info op_ane_o = { &op_ane, "ane" };
static struct op_info op_asl_o = { &op_asl, "asl" };
static struct op_info op_asr_o = { &op_asr, "asr" };
static struct op_info op_bcc_o = { &op_bcc, "bcc" };
static struct op_info op_bcs_o = { &op_bcs, "bcs" };
static struct op_info op_beq_o = { &op_beq, "beq" };
static struct op_info op_bit_o = { &op_bit, "bit" };
static struct op_info op_bmi_o = { &op_bmi, "bmi" };
static struct op_info op_bne_o = { &op_bne, "bne" };
static struct op_info op_bpl_o = { &op_bpl, "bpl" };
static struct op_info op_brk_o = { &op_brk, "brk" };
static struct op_info op_bvc_o = { &op_bvc, "bvc" };
static struct op_info op_bvs_o = { &op_bvs, "bvs" };
static struct op_info op_clc_o = { &op_clc, "clc" };

static struct op_info op_cld_o = { &op_cld, "cld" };
static struct op_info op_cli_o = { &op_cli, "cli" };
static struct op_info op_clv_o = { &op_clv, "clv" };
static struct op_info op_cmp_o = { &op_cmp, "cmp" };
static struct op_info op_cpx_o = { &op_cpx, "cpx" };
static struct op_info op_cpy_o = { &op_cpy, "cpy" };
static struct op_info op_dcp_o = { &op_dcp, "dcp" };
static struct op_info op_dec_o = { &op_dec, "dec" };
static struct op_info op_dex_o = { &op_dex, "dex" };
static struct op_info op_dey_o = { &op_dey, "dey" };
static struct op_info op_eor_o = { &op_eor, "eor" };
static struct op_info op_inc_o = { &op_inc, "inc" };
static struct op_info op_isb_o = { &op_isb, "isb" };
static struct op_info op_inx_o = { &op_inx, "inx" };
static struct op_info op_iny_o = { &op_iny, "iny" };
static struct op_info op_jmp_o = { &op_jmp, "jmp" };

static struct op_info op_jsr_o = { &op_jsr, "jsr" };
static struct op_info op_lda_o = { &op_lda, "lda" };
static struct op_info op_ldx_o = { &op_ldx, "ldx" };
static struct op_info op_ldy_o = { &op_ldy, "ldy" };
static struct op_info op_lsr_o = { &op_lsr, "lsr" };
static struct op_info op_nop_o = { &op_nop, "nop" };
static struct op_info op_ora_o = { &op_ora, "ora" };
static struct op_info op_pha_o = { &op_pha, "pha" };
static struct op_info op_php_o = { &op_php, "php" };
static struct op_info op_pla_o = { &op_pla, "pla" };
static struct op_info op_plp_o = { &op_plp, "plp" };
static struct op_info op_rol_o = { &op_rol, "rol" };
static struct op_info op_rla_o = { &op_rla, "rla" };
static struct op_info op_ror_o = { &op_ror, "ror" };
static struct op_info op_rti_o = { &op_rti, "rti" };
static struct op_info op_rra_o = { &op_rra, "rra" };

static struct op_info op_rts_o = { &op_rts, "rts" };
static struct op_info op_sbc_o = { &op_sbc, "sbc" };
static struct op_info op_sbx_o = { &op_sbx, "sbx" };
static struct op_info op_sec_o = { &op_sec, "sec" };
static struct op_info op_sed_o = { &op_sed, "sed" };
static struct op_info op_sei_o = { &op_sei, "sei" };
static struct op_info op_slo_o = { &op_slo, "slo" };
static struct op_info op_sta_o = { &op_sta, "sta" };
static struct op_info op_stx_o = { &op_stx, "stx" };
static struct op_info op_sty_o = { &op_sty, "sty" };
static struct op_info op_lax_o = { &op_lax, "lax" };
static struct op_info op_lae_o = { &op_lae, "lae" };
static struct op_info op_lxa_o = { &op_lxa, "lxa" };
static struct op_info op_tax_o = { &op_tax, "tax" };
static struct op_info op_sax_o = { &op_sax, "sax" };
static struct op_info op_sre_o = { &op_sre, "sre" };
static struct op_info op_tay_o = { &op_tay, "tay" };
static struct op_info op_tsx_o = { &op_tsx, "tsx" };
static struct op_info op_txa_o = { &op_txa, "txa" };
static struct op_info op_txs_o = { &op_txs, "txs" };
static struct op_info op_tya_o = { &op_tya, "tya" };

#define NULL_OP       \
    {                 \
        NULL, NULL, 0 \
    }

static int retfire = 0xff;
static int retspace = 0xff;
int flipfire(void)
{
    retfire ^= 0x90;
    return retfire;
}
int flipspace(void)
{
    retspace ^= 0x10;
    return retspace;
}

/* http://www.obelisk.demon.co.uk/6502/reference.html is a nice reference */
static struct inst_info ops[256] = {
    /* 0x00 */
    { &op_brk_o, &mode_imp_o, 7 },
    { &op_ora_o, &mode_indx_o, 6 },
    NULL_OP,
    NULL_OP,
    { &op_nop_o, &mode_zp_o, 3 }, /* $04 nop $ff */
    { &op_ora_o, &mode_zp_o, 3 },
    { &op_asl_o, &mode_zp_o, 5 },
    { &op_slo_o, &mode_zp_o, 5 }, /* $07 slo $ff */
    { &op_php_o, &mode_imp_o, 3 },
    { &op_ora_o, &mode_imm_o, 2 },
    { &op_asl_o, &mode_acc_o, 2 },
    { &op_anc_o, &mode_imm_o, 2 }, /* $0b anc #$ff */
    { &op_nop_o, &mode_abs_o, 4 }, /* $0c nop $ffff */
    { &op_ora_o, &mode_abs_o, 4 },
    { &op_asl_o, &mode_abs_o, 6 },
    { &op_slo_o, &mode_abs_o, 6 }, /* $0f slo $ffff */
    /* 0x10 */
    { &op_bpl_o, &mode_rel_o, 2 },
    { &op_ora_o, &mode_indy_o, 5 },
    NULL_OP,
    NULL_OP,
    { &op_nop_o, &mode_zpx_o, 4 }, /* $14 nop $ff,x */
    { &op_ora_o, &mode_zpx_o, 4 },
    { &op_asl_o, &mode_zpx_o, 6 },
    NULL_OP,
    { &op_clc_o, &mode_imp_o, 2 },
    { &op_ora_o, &mode_absy_o, 4 },
    { &op_nop_o, &mode_imp_o, 2 }, /* $1a nop */
    { &op_slo_o, &mode_absy_o, 4 }, /* $1b slo $ffff,y */
    { &op_nop_o, &mode_absx_o, 4 }, /* $1c nop $ffff,x */
    { &op_ora_o, &mode_absx_o, 4 },
    { &op_asl_o, &mode_absx_o, 7 },
    { &op_slo_o, &mode_absx_o, 7 }, /* $1f slo $ffff,x */
    /* 0x20 */
    { &op_jsr_o, &mode_abs_o, 6 },
    { &op_and_o, &mode_indx_o, 6 },
    NULL_OP,
    { &op_rla_o, &mode_indx_o, 8 }, /* $23 rla ($ff,x) */
    { &op_bit_o, &mode_zp_o, 3 },
    { &op_and_o, &mode_zp_o, 3 },
    { &op_rol_o, &mode_zp_o, 5 },
    { &op_rla_o, &mode_zp_o, 5 }, /* $27 rla $ff */
    { &op_plp_o, &mode_imp_o, 4 },
    { &op_and_o, &mode_imm_o, 2 },
    { &op_rol_o, &mode_acc_o, 2 },
    { &op_anc_o, &mode_imm_o, 2 }, /* $2b anc #$ff */
    { &op_bit_o, &mode_abs_o, 4 },
    { &op_and_o, &mode_abs_o, 4 },
    { &op_rol_o, &mode_abs_o, 6 },
    { &op_rla_o, &mode_abs_o, 6 }, /* $2f rla $ffff */
    /* 0x30 */
    { &op_bmi_o, &mode_rel_o, 2 },
    { &op_and_o, &mode_indy_o, 5 },
    NULL_OP,
    { &op_rla_o, &mode_indy_o, 8 }, /* $33 rla ($ff),y  */
    { &op_nop_o, &mode_zpx_o, 4 }, /* $34 nop $ff,x */
    { &op_and_o, &mode_zpx_o, 4 },
    { &op_rol_o, &mode_zpx_o, 6 },
    { &op_rla_o, &mode_zpx_o, 6 }, /* $37 rla $ff,x */
    { &op_sec_o, &mode_imp_o, 2 },
    { &op_and_o, &mode_absy_o, 4 },
    { &op_nop_o, &mode_imp_o, 2 }, /* $3a nop */
    { &op_rla_o, &mode_absy_o, 7 }, /* $3b rla $ffff,y */
    { &op_nop_o, &mode_absx_o, 7 }, /* $3c nop $ffff,x */
    { &op_and_o, &mode_absx_o, 4 },
    { &op_rol_o, &mode_absx_o, 7 },
    { &op_rla_o, &mode_absx_o, 7 }, /* $3f rla $ffff,x */
    /* 0x40 */
    { &op_rti_o, &mode_imp_o, 6 },
    { &op_eor_o, &mode_indx_o, 6 },
    NULL_OP,
    { &op_sre_o, &mode_indx_o, 6 }, /* $43 sre ($ff,x) */
    { &op_nop_o, &mode_zp_o, 3 }, /* $44 nop $ff */
    { &op_eor_o, &mode_zp_o, 3 },
    { &op_lsr_o, &mode_zp_o, 5 },
    { &op_sre_o, &mode_zp_o, 5 }, /* $47 sre $ff */
    { &op_pha_o, &mode_imp_o, 3 },
    { &op_eor_o, &mode_imm_o, 2 },
    { &op_lsr_o, &mode_acc_o, 2 },
    { &op_asr_o, &mode_imm_o, 2 }, /* $4b asr #$ff */
    { &op_jmp_o, &mode_abs_o, 3 },
    { &op_eor_o, &mode_abs_o, 4 },
    { &op_lsr_o, &mode_abs_o, 6 },
    { &op_sre_o, &mode_abs_o, 6 }, /* $4f sre $ffff */
    /* 0x50 */
    { &op_bvc_o, &mode_rel_o, 2 },
    { &op_eor_o, &mode_indy_o, 5 },
    NULL_OP,
    NULL_OP,
    { &op_nop_o, &mode_zpx_o, 4 }, /* $54 nop $ff,x */
    { &op_eor_o, &mode_zpx_o, 4 }, /* fix: eor $ff,x takes 4 cycles*/
    { &op_lsr_o, &mode_zpx_o, 6 },
    { &op_sre_o, &mode_zpx_o, 6 }, /* $57 sre $ff,x */
    { &op_cli_o, &mode_imp_o, 2 },
    { &op_eor_o, &mode_absy_o, 4 },
    { &op_nop_o, &mode_imp_o, 2 }, /* $5a nop */
    { &op_sre_o, &mode_absy_o, 7 }, /* $5b sre $ffff,y */
    { &op_nop_o, &mode_absx_o, 4 }, /* $5c nop $ffff,x */
    { &op_eor_o, &mode_absx_o, 4 },
    { &op_lsr_o, &mode_absx_o, 7 },
    { &op_sre_o, &mode_absx_o, 7 }, /* $5f sre $ffff,x */
    /* 0x60 */
    { &op_rts_o, &mode_imp_o, 6 },
    { &op_adc_o, &mode_indx_o, 6 },
    NULL_OP,
    { &op_rra_o, &mode_indx_o, 8 }, /* $63 rra ($ff,x) */
    { &op_nop_o, &mode_zp_o, 3 }, /* $64 nop $ff */
    { &op_adc_o, &mode_zp_o, 3 },
    { &op_ror_o, &mode_zp_o, 5 },
    { &op_rra_o, &mode_zp_o, 5 }, /* $67 rra $ff */
    { &op_pla_o, &mode_imp_o, 4 },
    { &op_adc_o, &mode_imm_o, 2 },
    { &op_ror_o, &mode_acc_o, 2 },
    NULL_OP, /* fix: $6b ARR (todo?) */
    { &op_jmp_o, &mode_ind_o, 5 }, /* fix: $6c JMP ($FFFF) */
    { &op_adc_o, &mode_abs_o, 4 },
    { &op_ror_o, &mode_abs_o, 6 },
    { &op_rra_o, &mode_abs_o, 6 }, /* $6f rra $ffff */
    /* 0x70 */
    { &op_bvs_o, &mode_rel_o, 2 },
    { &op_adc_o, &mode_indy_o, 5 },
    NULL_OP,
    { &op_rra_o, &mode_indy_o, 8 }, /* $73 rra ($ff),y */
    { &op_nop_o, &mode_zpx_o, 4 }, /* $74 nop $ff,x */
    { &op_adc_o, &mode_zpx_o, 4 },
    { &op_ror_o, &mode_zpx_o, 6 },
    { &op_rra_o, &mode_zpx_o, 6 }, /* $77 rra $ff,x */
    { &op_sei_o, &mode_imp_o, 2 },
    { &op_adc_o, &mode_absy_o, 4 },
    { &op_nop_o, &mode_imp_o, 2 }, /* $7a nop */
    { &op_rra_o, &mode_absy_o, 7 }, /* $7b rra $ffff,y */
    { &op_nop_o, &mode_absx_o, 4 }, /* $7c nop $ffff,x */
    { &op_adc_o, &mode_absx_o, 4 },
    { &op_ror_o, &mode_absx_o, 7 },
    { &op_rra_o, &mode_absx_o, 7 }, /* $7f rra $ffff,x */
    /* 0x80 */
    { &op_nop_o, &mode_imm_o, 2 }, /* $80 nop #$ff */
    { &op_sta_o, &mode_indx_o, 6 },
    { &op_nop_o, &mode_imm_o, 2 }, /* $82 nop #$ff */
    { &op_sax_o, &mode_indx_o, 6 }, /* $83 sax ($ff,x) */
    { &op_sty_o, &mode_zp_o, 3 },
    { &op_sta_o, &mode_zp_o, 3 },
    { &op_stx_o, &mode_zp_o, 3 },
    { &op_sax_o, &mode_zp_o, 3 }, /* $87 sax $ff */
    { &op_dey_o, &mode_imp_o, 2 },
    { &op_nop_o, &mode_imm_o, 2 }, /* $89 nop #$ff */
    { &op_txa_o, &mode_imp_o, 2 },
    { &op_ane_o, &mode_imm_o, 2 }, /* $8b ane #$ff */
    { &op_sty_o, &mode_abs_o, 4 },
    { &op_sta_o, &mode_abs_o, 4 },
    { &op_stx_o, &mode_abs_o, 4 },
    { &op_sax_o, &mode_abs_o, 4 }, /* $8f sax $ffff */
    /* 0x90 */
    { &op_bcc_o, &mode_rel_o, 2 },
    { &op_sta_o, &mode_indy_o, 6 },
    NULL_OP,
    NULL_OP,
    { &op_sty_o, &mode_zpx_o, 4 },
    { &op_sta_o, &mode_zpx_o, 4 },
    { &op_stx_o, &mode_zpy_o, 4 },
    { &op_sax_o, &mode_zpy_o, 4 }, /* $97 sax $ff,y */
    { &op_tya_o, &mode_imp_o, 2 },
    { &op_sta_o, &mode_absy_o, 5 },
    { &op_txs_o, &mode_imp_o, 2 },
    NULL_OP,
    NULL_OP,
    { &op_sta_o, &mode_absx_o, 5 },
    NULL_OP,
    NULL_OP,
    /* 0xa0 */
    { &op_ldy_o, &mode_imm_o, 2 },
    { &op_lda_o, &mode_indx_o, 6 },
    { &op_ldx_o, &mode_imm_o, 2 },
    { &op_lax_o, &mode_indx_o, 6 }, /* $a3 lax ($ff,x) */
    { &op_ldy_o, &mode_zp_o, 3 },
    { &op_lda_o, &mode_zp_o, 3 },
    { &op_ldx_o, &mode_zp_o, 3 },
    { &op_lax_o, &mode_zp_o, 3 }, /* $a7 lax $ff */
    { &op_tay_o, &mode_imp_o, 2 },
    { &op_lda_o, &mode_imm_o, 2 },
    { &op_tax_o, &mode_imp_o, 2 },
    { &op_lxa_o, &mode_imm_o, 2 }, /* $ab lxa #$ff */
    { &op_ldy_o, &mode_abs_o, 4 },
    { &op_lda_o, &mode_abs_o, 4 },
    { &op_ldx_o, &mode_abs_o, 4 },
    { &op_lax_o, &mode_abs_o, 4 }, /* $af lax $ffff */
    /* 0xb0 */
    { &op_bcs_o, &mode_rel_o, 2 },
    { &op_lda_o, &mode_indy_o, 5 },
    NULL_OP,
    { &op_lax_o, &mode_indy_o, 5 }, /* $b3 lax ($ff),y */
    { &op_ldy_o, &mode_zpx_o, 4 },
    { &op_lda_o, &mode_zpx_o, 4 },
    { &op_ldx_o, &mode_zpy_o, 4 },
    { &op_lax_o, &mode_zpy_o, 4 }, /* $b7 lax $ff,y */
    { &op_clv_o, &mode_imp_o, 2 },
    { &op_lda_o, &mode_absy_o, 4 },
    { &op_tsx_o, &mode_imp_o, 2 },
    { &op_lae_o, &mode_absy_o, 4 }, /* $bb lae $ffff,y */
    { &op_ldy_o, &mode_absx_o, 4 },
    { &op_lda_o, &mode_absx_o, 4 },
    { &op_ldx_o, &mode_absy_o, 4 },
    { &op_lax_o, &mode_absy_o, 4 }, /* $bf lax $ffff,y */
    /* 0xc0 */
    { &op_cpy_o, &mode_imm_o, 2 },
    { &op_cmp_o, &mode_indx_o, 6 },
    { &op_nop_o, &mode_imm_o, 2 }, /* $c2 nop #$ff */
    { &op_dcp_o, &mode_indx_o, 8 }, /* $c3 dcp ($ff,x) */
    { &op_cpy_o, &mode_zp_o, 3 },
    { &op_cmp_o, &mode_zp_o, 3 },
    { &op_dec_o, &mode_zp_o, 5 },
    { &op_dcp_o, &mode_zp_o, 5 }, /* $c7 dcp $FF */
    { &op_iny_o, &mode_imp_o, 2 },
    { &op_cmp_o, &mode_imm_o, 2 },
    { &op_dex_o, &mode_imp_o, 2 },
    { &op_sbx_o, &mode_imm_o, 2 }, /* $cb sbx #$ff */
    { &op_cpy_o, &mode_abs_o, 4 },
    { &op_cmp_o, &mode_abs_o, 4 },
    { &op_dec_o, &mode_abs_o, 6 },
    { &op_dcp_o, &mode_abs_o, 6 }, /* $cf dcp $ffff */
    /* 0xd0 */
    { &op_bne_o, &mode_rel_o, 2 },
    { &op_cmp_o, &mode_indy_o, 5 },
    NULL_OP,
    { &op_dcp_o, &mode_indy_o, 8 }, /* $d3 dcp ($ff),y */
    { &op_nop_o, &mode_zpx_o, 4 }, /* $d4 nop $ff,x */
    { &op_cmp_o, &mode_zpx_o, 4 },
    { &op_dec_o, &mode_zpx_o, 6 },
    { &op_dcp_o, &mode_zpx_o, 6 }, /* $d7 dcp $ff,x */
    { &op_cld_o, &mode_imp_o, 2 },
    { &op_cmp_o, &mode_absy_o, 4 },
    { &op_nop_o, &mode_imp_o, 2 }, /* $da nop */
    { &op_dcp_o, &mode_absy_o, 7 }, /* $db dcp $ffff,y */
    { &op_nop_o, &mode_absx_o, 4 }, /* $dc nop $ffff,x */
    { &op_cmp_o, &mode_absx_o, 4 },
    { &op_dec_o, &mode_absx_o, 7 },
    { &op_dcp_o, &mode_absx_o, 7 }, /* $df dcp $ffff,x */
    /* 0xe0 */
    { &op_cpx_o, &mode_imm_o, 2 },
    { &op_sbc_o, &mode_indx_o, 6 },
    { &op_nop_o, &mode_imm_o, 2 }, /* $e2 nop #$ff */
    { &op_isb_o, &mode_indx_o, 8 }, /* $e3 isb ($ff,x) */
    { &op_cpx_o, &mode_zp_o, 3 },
    { &op_sbc_o, &mode_zp_o, 3 },
    { &op_inc_o, &mode_zp_o, 5 },
    { &op_isb_o, &mode_zp_o, 5 }, /* $e7 isb $ff */
    { &op_inx_o, &mode_imp_o, 2 },
    { &op_sbc_o, &mode_imm_o, 2 },
    { &op_nop_o, &mode_imp_o, 2 },
    { &op_sbc_o, &mode_imm_o, 2 }, /* $eb sbc #$ff */
    { &op_cpx_o, &mode_abs_o, 4 },
    { &op_sbc_o, &mode_abs_o, 4 },
    { &op_inc_o, &mode_abs_o, 6 },
    { &op_isb_o, &mode_abs_o, 6 }, /* $ef isb $ffff */
    /* 0xf0 */
    { &op_beq_o, &mode_rel_o, 2 },
    { &op_sbc_o, &mode_indy_o, 5 },
    NULL_OP,
    { &op_isb_o, &mode_indy_o, 8 }, /* $f3 isb ($ff),y */
    { &op_nop_o, &mode_zpx_o, 4 }, /* $f4 nop $ff,x */
    { &op_sbc_o, &mode_zpx_o, 4 },
    { &op_inc_o, &mode_zpx_o, 6 },
    { &op_isb_o, &mode_zpx_o, 6 }, /* $f7 isb $ff,x */
    { &op_sed_o, &mode_imp_o, 2 },
    { &op_sbc_o, &mode_absy_o, 4 },
    { &op_nop_o, &mode_imp_o, 2 }, /* $fa nop */
    { &op_isb_o, &mode_absy_o, 7 }, /* $fb isb $ffff,y */
    { &op_nop_o, &mode_absx_o, 7 }, /* $fc nop $ffff,x */
    { &op_sbc_o, &mode_absx_o, 4 },
    { &op_inc_o, &mode_absx_o, 7 },
    { &op_isb_o, &mode_absx_o, 7 }, /* $ff isb $ffff,x */
};

int byted011[2] = { 0, 0 };
int next_inst(struct cpu_ctx *r)
{
    union inst_arg arg[1];
    int oldpc = r->pc;
    int op_code = r->mem[r->pc];
    struct inst_info *info = ops + op_code;
    int mode, WriteToIO = 0;
    int bt = 0, br;
    if (info->op == NULL) {
        fprintf(stderr, "unimplemented opcode $%02X @ $%04X\n", op_code, r->pc);
        return 1;
        //        exit(1);
    }
    //    LOG(LOG_DUMP, ("%02x %02x %02x %02x %02x: ", r->a, r->x, r->y,
    //    r->sp,r->flags));
    mode = info->mode->f(r, arg);

    /* iAN: only if RAM is not visible there! */
    if (((r->mem[1] & 0x7) >= 0x5) && ((r->mem[1] & 0x07) <= 0x7)) {
        if (arg->ea.value >= 0xd000 && arg->ea.value < 0xe000) {
            /* is this an absolute sta, stx or sty to the IO-area? */
            /* iAN: added all possible writing opcodes */
            if (op_code == 0x0E || /*ASL $ffff  */
                op_code == 0x1E || /*ASL $ffff,X*/
                op_code == 0xCE || /*DEC $ffff  */
                op_code == 0xDE || /*DEC $ffff,X*/
                op_code == 0xEE || /*INC $ffff  */
                op_code == 0xFE || /*INC $ffff,X*/
                op_code == 0x4E || /*LSR $ffff  */
                op_code == 0x5E || /*LSR $ffff,X*/
                op_code == 0x2E || /*ROL $ffff  */
                op_code == 0x3E || /*ROL $ffff,X*/
                op_code == 0x6E || /*ROR $ffff  */
                op_code == 0x7E || /*ROR $ffff,X*/
                op_code == 0x8D || /*STA $ffff  */
                op_code == 0x9D || /*STA $ffff,X*/
                op_code == 0x99 || /*STA $ffff,Y*/
                op_code == 0x91 || /*STA ($ff),Y*/
                op_code == 0x8E || /*STX $ffff  */
                op_code == 0x8C /*STY $ffff  */
            ) {
                r->cycles += ops[op_code].cycles;
                /* ignore it, its probably an effect */
                /* try to keep updated at least a copy of $d011 */
                if (arg->ea.value == 0xd011) {
                    switch (op_code) {
                    case 0x8D:
                        byted011[0] = r->a & 0x7f;
                        break;
                    case 0x8E:
                        byted011[0] = r->x & 0x7f;
                        break;
                    case 0x8C:
                        byted011[0] = r->y & 0x7f;
                        break;
                    default:
                        break;
                    }
                }
                WriteToIO = 1;
            } else {
                byted011[1] = (r->cycles / 0x3f) % 0x157;
                byted011[0] = (byted011[0] & 0x7f) | ((byted011[1] & 0x100) >> 1);
                byted011[1] &= 0xff;
                switch (op_code) {
                case 0xad:
                case 0xaf: /* lda $ffff / lax $ffff */

                    if ((arg->ea.value == 0xd011) || (arg->ea.value == 0xd012)) {
                        r->cycles += ops[op_code].cycles;
                        r->a = byted011[arg->ea.value - 0xd011];
                        if (op_code == 0xaf)
                            r->x = r->a;
                        update_flags_nz(r, r->a);
                        WriteToIO = 5;
                        break;
                    }

                    /* intros: simulate space */
                    if (arg->ea.value == 0xdc00 || arg->ea.value == 0xdc01) {
                        r->cycles += ops[op_code].cycles;
                        if (arg->ea.value == 0xdc00)
                            r->a = flipfire();
                        else
                            r->a = flipspace();
                        if (op_code == 0xaf)
                            r->x = r->a;
                        update_flags_nz(r, r->a);
                        WriteToIO = 6;
                        break;
                    }

                    if (arg->ea.value >= 0xdd01 && arg->ea.value <= 0xdd0f) {
                        r->cycles += ops[op_code].cycles;
                        r->a = 0xff;
                        if (op_code == 0xaf)
                            r->x = r->a;
                        update_flags_nz(r, r->a);
                        WriteToIO = 2;
                        break;
                    }
                    break;
                case 0x2d: /* and $ffff */

                    /* intros: simulate space */
                    if (arg->ea.value == 0xdc00 || arg->ea.value == 0xdc01) {
                        r->cycles += ops[op_code].cycles;
                        if (arg->ea.value == 0xdc00)
                            r->a &= flipfire();
                        else
                            r->a &= flipspace();

                        update_flags_nz(r, r->a);
                        WriteToIO = 6;
                        break;
                    }
                    break;
                case 0xae: /* ldx $ffff */

                    if ((arg->ea.value == 0xd011) || (arg->ea.value == 0xd012)) {
                        r->cycles += ops[op_code].cycles;
                        r->x = byted011[arg->ea.value - 0xd011];
                        update_flags_nz(r, r->x);
                        WriteToIO = 5;
                        break;
                    }
                    if (arg->ea.value == 0xdc00 || arg->ea.value == 0xdc01) {
                        r->cycles += ops[op_code].cycles;
                        if (arg->ea.value == 0xdc00)
                            r->x = flipfire();
                        else
                            r->x = flipspace();
                        update_flags_nz(r, r->x);
                        WriteToIO = 6;
                        break;
                    }
                    break;
                case 0xac: /* ldy $ffff */

                    if ((arg->ea.value == 0xd011) || (arg->ea.value == 0xd012)) {
                        r->cycles += ops[op_code].cycles;
                        r->y = byted011[arg->ea.value - 0xd011];
                        update_flags_nz(r, r->y);
                        WriteToIO = 5;
                        break;
                    }
                    if (arg->ea.value == 0xdc00 || arg->ea.value == 0xdc01) {
                        r->cycles += ops[op_code].cycles;
                        if (arg->ea.value == 0xdc00)
                            r->y = flipfire();
                        else
                            r->y = flipspace();
                        update_flags_nz(r, r->y);
                        WriteToIO = 6;
                        break;
                    }
                    break;

                case 0x2c: /* bit $d011 */

                    if ((arg->ea.value == 0xd011) || (arg->ea.value == 0xd012)) {
                        r->cycles += ops[op_code].cycles;
                        bt = byted011[arg->ea.value - 0xd011];
                        r->flags &= ~(FLAG_N | FLAG_V | FLAG_Z);
                        r->flags |= (bt & FLAG_N) != 0 ? FLAG_N : 0;
                        r->flags |= (bt & FLAG_V) != 0 ? FLAG_V : 0;
                        r->flags |= (bt & r->a) == 0 ? FLAG_Z : 0;
                        WriteToIO = 3;
                    }
                    break;

                case 0xcd: /* cmp $ffff */
                case 0xec: /* cpx $ffff */
                case 0xcc: /* cpy $ffff */
                    if ((arg->ea.value == 0xd011) || (arg->ea.value == 0xd012)) {
                        r->cycles += ops[op_code].cycles;
                        bt = byted011[arg->ea.value - 0xd011];
                        br = r->a;
                        if (op_code == 0xec)
                            br = r->x;
                        if (op_code == 0xcc)
                            br = r->y;
                        subtract(r, 1, br, bt);
                        WriteToIO = 4;
                        break;
                    }
                    /* intros: simulate space */
                    if (arg->ea.value == 0xdc00 || arg->ea.value == 0xdc01) {
                        r->cycles += ops[op_code].cycles;
                        br = r->a;
                        if (op_code == 0xec)
                            br = r->x;
                        if (op_code == 0xcc)
                            br = r->y;
                        if (arg->ea.value == 0xdc00)
                            bt = flipfire();
                        else
                            bt = flipspace();
                        subtract(r, 1, br, bt);
                        WriteToIO = 6;
                        break;
                    }
                    break;
                }
            }
        }
    }

    if (IS_LOGGABLE(LOG_DUMP)) {
        //        LOG(LOG_DUMP, ("%04x %s", oldpc, info->op->fmt));

        if (info->mode->fmt != NULL) {
            int value = 0;
            int pc = r->pc;
            while (--pc > oldpc) {
                value <<= 8;
                value |= r->mem[pc];
            }
            //            LOG(LOG_DUMP, (" "));
            if (mode == MODE_RELATIVE) {
                //                LOG(LOG_DUMP, ("$%04x", r->pc + arg->rel.value));
            } else {
                //                LOG(LOG_DUMP, (info->mode->fmt, value));
            }
            switch (mode) {
            case MODE_RELATIVE:
            case MODE_IMPLIED:
            case MODE_ACCUMULATOR:
            case MODE_IMMEDIATE:
            case MODE_ABSOLUTE:
                break;
            default:
                fprintf(stderr, " ($%04x)", arg->ea.value);
            }
        }
        switch (WriteToIO) {
        case 1:
            //            LOG(LOG_DUMP, (" write to IO, skipped\n"));
            break;
        }
        //        LOG(LOG_DUMP, ("\n"));
    }
    if (WriteToIO) {
        if (debugprint) {
            switch (WriteToIO) {
            case 2:
                fprintf(stderr, "\nLDA $%04X -> $ff forced\n", arg->ea.value);
                break;
            case 6:
                fprintf(stderr, "\nLDA $DC01->space %s\n",
                    (retspace == 0xef ? "pressed" : "released"));
                break;
            }
        }
        return 0;
    }
    info->op->f(r, mode, arg);
    r->cycles += info->cycles;
    if (op_code == 0) {
        fprintf(stderr, "\nBRK reached @ $%04X\n", r->pc - 1);
        //        exit(1);
        return 1;
    }
    return 0;
}
