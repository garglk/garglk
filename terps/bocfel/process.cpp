// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <array>
#include <functional>

#ifdef ZTERP_GLK_TICK
extern "C" {
#include <glk.h>
}
#endif

#include "process.h"
#include "branch.h"
#include "dict.h"
#include "mathop.h"
#include "memory.h"
#include "objects.h"
#include "options.h"
#include "random.h"
#include "screen.h"
#include "sound.h"
#include "stack.h"
#include "types.h"
#include "util.h"
#include "zoom.h"
#include "zterp.h"

unsigned long pc;
unsigned long current_instruction;

std::array<uint16_t, 8> zargs;
int znargs;

// Track the current processing level: 1 for the “main” loop, 2 if
// inside of an interrupt, 3 if inside of an interrupt inside of an
// interrupt, and so on.
static int processing_level = 0;

// In general an “internal” call is used for interrupts, and saving is
// not allowed in an interrupt. But the @shogun_menu hack has to use an
// internal call as well, and in that call, saving must work. This flag
// indicates that the current internal call is not actually an
// interrupt. Theoretically this is insufficient because an interrupt
// could be called while inside of @shogun_menu’s internal call, but
// that doesn’t actually happen.
bool interrupt_override = false;

bool in_interrupt()
{
    return processing_level > 1 && !interrupt_override;
}

// Returns true if decoded, false otherwise (omitted)
static bool decode_base(uint8_t type, uint16_t &loc)
{
    switch (type) {
    case 0: // Large constant.
        loc = word(pc);
        pc += 2;
        break;
    case 1: // Small constant.
        loc = byte(pc++);
        break;
    case 2: // Variable.
        loc = variable(byte(pc++));
        break;
    default: // Omitted.
        return false;
    }

    return true;
}

static void decode_var(uint8_t types)
{
    uint16_t ret;

    for (int i = 6; i >= 0; i -= 2) {
        if (!decode_base((types >> i) & 0x03, ret)) {
            return;
        }
        zargs[znargs++] = ret;
    }
}

static std::array<void(*)(), 256> opcodes;
static std::array<void(*)(), 256> ext_opcodes;

enum class Opcount {
    Zero,
    One,
    Two,
    Var,
    Ext,
};

#define op_call(opcode)		opcodes[opcode]()
#define extended_call(opcode)	ext_opcodes[opcode]()

// This nifty trick is from Frotz.
static void zextended()
{
    uint8_t opnumber = byte(pc++);

    decode_var(byte(pc++));

    extended_call(opnumber);
}

[[noreturn]]
static void illegal_opcode()
{
    die("illegal opcode (pc = 0x%lx)", current_instruction);
}

static void setup_single_opcode(int minver, int maxver, Opcount opcount, int opcode, void (*fn)())
{
    if (zversion < minver || zversion > maxver) {
        return;
    }

    switch (opcount) {
    case Opcount::Zero:
        opcodes[opcode + 176] = fn;
        break;
    case Opcount::One:
        opcodes[opcode + 128] = fn;
        opcodes[opcode + 144] = fn;
        opcodes[opcode + 160] = fn;
        break;
    case Opcount::Two:
        opcodes[opcode +   0] = fn;
        opcodes[opcode +  32] = fn;
        opcodes[opcode +  64] = fn;
        opcodes[opcode +  96] = fn;
        opcodes[opcode + 192] = fn;
        break;
    case Opcount::Var:
        opcodes[opcode + 224] = fn;
        break;
    case Opcount::Ext:
        ext_opcodes[opcode] = fn;
        break;
    }
}

void setup_opcodes()
{
    opcodes.fill(illegal_opcode);

    // §14.2.1
    ext_opcodes.fill(znop);

    setup_single_opcode(1, 6, Opcount::Zero, 0x00, zrtrue);
    setup_single_opcode(1, 6, Opcount::Zero, 0x01, zrfalse);
    setup_single_opcode(1, 6, Opcount::Zero, 0x02, zprint);
    setup_single_opcode(1, 6, Opcount::Zero, 0x03, zprint_ret);
    setup_single_opcode(1, 6, Opcount::Zero, 0x04, znop);
    setup_single_opcode(1, 4, Opcount::Zero, 0x05, zsave);
    setup_single_opcode(1, 4, Opcount::Zero, 0x06, zrestore);
    setup_single_opcode(1, 6, Opcount::Zero, 0x07, zrestart);
    setup_single_opcode(1, 6, Opcount::Zero, 0x08, zret_popped);
    setup_single_opcode(1, 4, Opcount::Zero, 0x09, zpop);
    setup_single_opcode(5, 6, Opcount::Zero, 0x09, zcatch);
    setup_single_opcode(1, 6, Opcount::Zero, 0x0a, zquit);
    setup_single_opcode(1, 6, Opcount::Zero, 0x0b, znew_line);
    setup_single_opcode(3, 3, Opcount::Zero, 0x0c, zshow_status);
    setup_single_opcode(3, 6, Opcount::Zero, 0x0d, zverify);
    setup_single_opcode(5, 6, Opcount::Zero, 0x0e, zextended);
    setup_single_opcode(5, 6, Opcount::Zero, 0x0f, zpiracy);

    setup_single_opcode(1, 6, Opcount::One, 0x00, zjz);
    setup_single_opcode(1, 6, Opcount::One, 0x01, zget_sibling);
    setup_single_opcode(1, 6, Opcount::One, 0x02, zget_child);
    setup_single_opcode(1, 6, Opcount::One, 0x03, zget_parent);
    setup_single_opcode(1, 6, Opcount::One, 0x04, zget_prop_len);
    setup_single_opcode(1, 6, Opcount::One, 0x05, zinc);
    setup_single_opcode(1, 6, Opcount::One, 0x06, zdec);
    setup_single_opcode(1, 6, Opcount::One, 0x07, zprint_addr);
    setup_single_opcode(4, 6, Opcount::One, 0x08, zcall_1s);
    setup_single_opcode(1, 6, Opcount::One, 0x09, zremove_obj);
    setup_single_opcode(1, 6, Opcount::One, 0x0a, zprint_obj);
    setup_single_opcode(1, 6, Opcount::One, 0x0b, zret);
    setup_single_opcode(1, 6, Opcount::One, 0x0c, zjump);
    setup_single_opcode(1, 6, Opcount::One, 0x0d, zprint_paddr);
    setup_single_opcode(1, 6, Opcount::One, 0x0e, zload);
    setup_single_opcode(1, 4, Opcount::One, 0x0f, znot);
    setup_single_opcode(5, 6, Opcount::One, 0x0f, zcall_1n);

    setup_single_opcode(1, 6, Opcount::Two, 0x01, zje);
    setup_single_opcode(1, 6, Opcount::Two, 0x02, zjl);
    setup_single_opcode(1, 6, Opcount::Two, 0x03, zjg);
    setup_single_opcode(1, 6, Opcount::Two, 0x04, zdec_chk);
    setup_single_opcode(1, 6, Opcount::Two, 0x05, zinc_chk);
    setup_single_opcode(1, 6, Opcount::Two, 0x06, zjin);
    setup_single_opcode(1, 6, Opcount::Two, 0x07, ztest);
    setup_single_opcode(1, 6, Opcount::Two, 0x08, zor);
    setup_single_opcode(1, 6, Opcount::Two, 0x09, zand);
    setup_single_opcode(1, 6, Opcount::Two, 0x0a, ztest_attr);
    setup_single_opcode(1, 6, Opcount::Two, 0x0b, zset_attr);
    setup_single_opcode(1, 6, Opcount::Two, 0x0c, zclear_attr);
    setup_single_opcode(1, 6, Opcount::Two, 0x0d, zstore);
    setup_single_opcode(1, 6, Opcount::Two, 0x0e, zinsert_obj);
    setup_single_opcode(1, 6, Opcount::Two, 0x0f, zloadw);
    setup_single_opcode(1, 6, Opcount::Two, 0x10, zloadb);
    setup_single_opcode(1, 6, Opcount::Two, 0x11, zget_prop);
    setup_single_opcode(1, 6, Opcount::Two, 0x12, zget_prop_addr);
    setup_single_opcode(1, 6, Opcount::Two, 0x13, zget_next_prop);
    setup_single_opcode(1, 6, Opcount::Two, 0x14, zadd);
    setup_single_opcode(1, 6, Opcount::Two, 0x15, zsub);
    setup_single_opcode(1, 6, Opcount::Two, 0x16, zmul);
    setup_single_opcode(1, 6, Opcount::Two, 0x17, zdiv);
    setup_single_opcode(1, 6, Opcount::Two, 0x18, zmod);
    setup_single_opcode(4, 6, Opcount::Two, 0x19, zcall_2s);
    setup_single_opcode(5, 6, Opcount::Two, 0x1a, zcall_2n);
    setup_single_opcode(5, 6, Opcount::Two, 0x1b, zset_colour);
    setup_single_opcode(5, 6, Opcount::Two, 0x1c, zthrow);

    setup_single_opcode(1, 6, Opcount::Var, 0x00, zcall);
    setup_single_opcode(1, 6, Opcount::Var, 0x01, zstorew);
    setup_single_opcode(1, 6, Opcount::Var, 0x02, zstoreb);
    setup_single_opcode(1, 6, Opcount::Var, 0x03, zput_prop);
    setup_single_opcode(1, 6, Opcount::Var, 0x04, zread);
    setup_single_opcode(1, 6, Opcount::Var, 0x05, zprint_char);
    setup_single_opcode(1, 6, Opcount::Var, 0x06, zprint_num);
    setup_single_opcode(1, 6, Opcount::Var, 0x07, zrandom);
    setup_single_opcode(1, 6, Opcount::Var, 0x08, zpush);
    setup_single_opcode(1, 6, Opcount::Var, 0x09, zpull);
    setup_single_opcode(3, 6, Opcount::Var, 0x0a, zsplit_window);
    setup_single_opcode(3, 6, Opcount::Var, 0x0b, zset_window);
    setup_single_opcode(4, 6, Opcount::Var, 0x0c, zcall_vs2);
    setup_single_opcode(4, 6, Opcount::Var, 0x0d, zerase_window);
    setup_single_opcode(4, 6, Opcount::Var, 0x0e, zerase_line);
    setup_single_opcode(4, 6, Opcount::Var, 0x0f, zset_cursor);
    setup_single_opcode(4, 6, Opcount::Var, 0x10, zget_cursor);
    setup_single_opcode(4, 6, Opcount::Var, 0x11, zset_text_style);
    setup_single_opcode(4, 6, Opcount::Var, 0x12, znop); // XXX buffer_mode
    setup_single_opcode(3, 6, Opcount::Var, 0x13, zoutput_stream);
    setup_single_opcode(3, 6, Opcount::Var, 0x14, zinput_stream);
    setup_single_opcode(3, 6, Opcount::Var, 0x15, zsound_effect);
    setup_single_opcode(4, 6, Opcount::Var, 0x16, zread_char);
    setup_single_opcode(4, 6, Opcount::Var, 0x17, zscan_table);
    setup_single_opcode(5, 6, Opcount::Var, 0x18, znot);
    setup_single_opcode(5, 6, Opcount::Var, 0x19, zcall_vn);
    setup_single_opcode(5, 6, Opcount::Var, 0x1a, zcall_vn2);
    setup_single_opcode(5, 6, Opcount::Var, 0x1b, ztokenise);
    setup_single_opcode(5, 6, Opcount::Var, 0x1c, zencode_text);
    setup_single_opcode(5, 6, Opcount::Var, 0x1d, zcopy_table);
    setup_single_opcode(5, 6, Opcount::Var, 0x1e, zprint_table);
    setup_single_opcode(5, 6, Opcount::Var, 0x1f, zcheck_arg_count);

    setup_single_opcode(5, 6, Opcount::Ext, 0x00, zsave5);
    setup_single_opcode(5, 6, Opcount::Ext, 0x01, zrestore5);
    setup_single_opcode(5, 6, Opcount::Ext, 0x02, zlog_shift);
    setup_single_opcode(5, 6, Opcount::Ext, 0x03, zart_shift);
    setup_single_opcode(5, 6, Opcount::Ext, 0x04, zset_font);
    setup_single_opcode(6, 6, Opcount::Ext, 0x05, zdraw_picture);
    setup_single_opcode(6, 6, Opcount::Ext, 0x06, zpicture_data);
    setup_single_opcode(6, 6, Opcount::Ext, 0x07, znop); // XXX erase_picture
    setup_single_opcode(6, 6, Opcount::Ext, 0x08, znop); // XXX set_margins
    setup_single_opcode(5, 6, Opcount::Ext, 0x09, zsave_undo);
    setup_single_opcode(5, 6, Opcount::Ext, 0x0a, zrestore_undo);
    setup_single_opcode(5, 6, Opcount::Ext, 0x0b, zprint_unicode);
    setup_single_opcode(5, 6, Opcount::Ext, 0x0c, zcheck_unicode);
    setup_single_opcode(5, 6, Opcount::Ext, 0x0d, zset_true_colour);
    setup_single_opcode(6, 6, Opcount::Ext, 0x10, znop); // XXX move_window
    setup_single_opcode(6, 6, Opcount::Ext, 0x11, znop); // XXX window_size
    setup_single_opcode(6, 6, Opcount::Ext, 0x12, znop); // XXX window_style
    setup_single_opcode(6, 6, Opcount::Ext, 0x13, zget_wind_prop);
    setup_single_opcode(6, 6, Opcount::Ext, 0x14, znop); // XXX scroll_window
    setup_single_opcode(6, 6, Opcount::Ext, 0x15, zpop_stack);
    setup_single_opcode(6, 6, Opcount::Ext, 0x16, znop); // XXX read_mouse
    setup_single_opcode(6, 6, Opcount::Ext, 0x17, znop); // XXX mouse_window
    setup_single_opcode(6, 6, Opcount::Ext, 0x18, zpush_stack);
    setup_single_opcode(6, 6, Opcount::Ext, 0x19, znop); // XXX put_wind_prop
    setup_single_opcode(6, 6, Opcount::Ext, 0x1a, zprint_form);
    setup_single_opcode(6, 6, Opcount::Ext, 0x1b, zmake_menu);
    setup_single_opcode(6, 6, Opcount::Ext, 0x1c, znop); // XXX picture_table
    setup_single_opcode(6, 6, Opcount::Ext, 0x1d, zbuffer_screen);

    // Zoom extensions.
    setup_single_opcode(5, 6, Opcount::Ext, 0x80, zstart_timer);
    setup_single_opcode(5, 6, Opcount::Ext, 0x81, zstop_timer);
    setup_single_opcode(5, 6, Opcount::Ext, 0x82, zread_timer);
    setup_single_opcode(5, 6, Opcount::Ext, 0x83, zprint_timer);

    // V6 hacks.
    setup_single_opcode(6, 6, Opcount::Ext, JOURNEY_DIAL_EXT, zjourney_dial);
    setup_single_opcode(6, 6, Opcount::Ext, SHOGUN_MENU_EXT, zshogun_menu);
}

// The main processing loop. This decodes and dispatches instructions.
// It will be called both at program start and whenever a @read or
// @read_char interrupt routine is called.
void process_instructions()
{
    static bool handled_autosave = false;

    if (options.autosave && !handled_autosave) {
        SaveOpcode saveopcode;

        handled_autosave = true;

        if (do_restore(SaveType::Autosave, saveopcode)) {
            show_message("Continuing last session from autosave");
            throw Operation::Restore(saveopcode);
        }
    }

    processing_level++;

    while (true) {
        uint8_t opcode;

#ifdef ZTERP_GLK_TICK
        glk_tick();
#endif

        current_instruction = pc;
        opcode = byte(pc++);

        if (opcode < 0x80) { // long 2OP
            znargs = 2;

            if ((opcode & 0x40) == 0x40) {
                zargs[0] = variable(byte(pc++));
            } else {
                zargs[0] = byte(pc++);
            }

            if ((opcode & 0x20) == 0x20) {
                zargs[1] = variable(byte(pc++));
            } else {
                zargs[1] = byte(pc++);
            }
        } else if (opcode < 0xb0) { // short 1OP
            znargs = 1;

            if ((opcode & 0x20) == 0x20) {
                zargs[0] = variable(byte(pc++));
            } else if ((opcode & 0x10) == 0x10) {
                zargs[0] = byte(pc++);
            } else {
                zargs[0] = word(pc);
                pc += 2;
            }
        } else if (opcode < 0xc0) { // short 0OP (plus EXT)
            znargs = 0;
        } else if (opcode == 0xec || opcode == 0xfa) { // Double variable VAR
            uint8_t types1, types2;

            znargs = 0;

            types1 = byte(pc++);
            types2 = byte(pc++);
            decode_var(types1);
            decode_var(types2);
        } else { // variable 2OP and VAR
            znargs = 0;

            decode_var(byte(pc++));
        }

        try {
            op_call(opcode);
        } catch (const Operation::Return &) {
            processing_level--;
            return;
        }
    }
}

// A wrapper around process_instructions() which is responsible for
// dealing with restart, restore, and quit.
void process_loop()
{
    std::function<void()> synthetic_call = nullptr;

    while (true) {
        try {
            if (synthetic_call != nullptr) {
                synthetic_call();
                synthetic_call = nullptr;
            }

            processing_level = 0;
            process_instructions();
        } catch (const Operation::Restart &) {
            start_story();
        } catch (const Operation::Restore &restore) {
            if (restore.saveopcode == SaveOpcode::Read) {
                synthetic_call = zread;
            } else if (restore.saveopcode == SaveOpcode::ReadChar) {
                synthetic_call = zread_char;
            }
        } catch (const Operation::Quit &) {
            break;
        }
    }
}
