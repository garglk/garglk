// Copyright 2010-2021 Chris Spiegel.
//
// This file is part of Bocfel.
//
// Bocfel is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version
// 2 or 3, as published by the Free Software Foundation.
//
// Bocfel is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Bocfel. If not, see <http://www.gnu.org/licenses/>.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#include "stack.h"
#include "branch.h"
#include "iff.h"
#include "io.h"
#include "memory.h"
#include "osdep.h"
#include "process.h"
#include "random.h"
#include "screen.h"
#include "stash.h"
#include "util.h"
#include "zterp.h"

typedef enum {
    StoreWhere_Variable,
    StoreWhere_None,
    StoreWhere_Push,
} StoreWhere;

struct call_frame {
    uint32_t pc;
    uint16_t *sp;
    uint8_t nlocals;
    uint8_t nargs;
    uint16_t where;
    uint16_t locals[15];
};

static struct call_frame *frames;
static struct call_frame *fp;

#define BASE_OF_FRAMES	frames
static struct call_frame *TOP_OF_FRAMES;
#define CURRENT_FRAME	(fp - 1)
#define NFRAMES		((long)(fp - frames))

static uint16_t *stack;
static uint16_t *sp;

#define BASE_OF_STACK	stack
static uint16_t *TOP_OF_STACK;

static void push_stack(uint16_t n)
{
    ZASSERT(sp != TOP_OF_STACK, "stack overflow");
    *sp++ = n;
}

static uint16_t pop_stack(void)
{
    ZASSERT(sp > CURRENT_FRAME->sp, "stack underflow");
    return *--sp;
}

struct save_state {
    enum SaveType savetype;
    uint8_t *quetzal;
    long quetzal_size;

    char *desc;

    struct save_state *prev, *next;
};

static struct save_stack
{
    struct save_state *head;
    struct save_state *tail;

    long max;
    long count;
} save_stacks[2];

bool seen_save_undo = false;

static void add_frame(uint32_t pc_, uint16_t *sp_, uint8_t nlocals, uint8_t nargs, uint16_t where)
{
    ZASSERT(fp != TOP_OF_FRAMES, "call stack too deep: %ld", NFRAMES + 1);

    fp->pc = pc_;
    fp->sp = sp_;
    fp->nlocals = nlocals;
    fp->nargs = nargs;
    fp->where = where;

    fp++;
}

static struct save_state *new_save_state(enum SaveType savetype, const char *desc)
{
    struct save_state *new;

    new = malloc(sizeof *new);
    if (new != NULL) {
        char formatted_time[128];

        new->savetype = savetype;
        new->quetzal = NULL;
        new->quetzal_size = 0;
        new->desc = NULL;

        if (desc == NULL) {
            struct tm *tm = localtime(&(time_t){ time(NULL) });

            if (tm != NULL && strftime(formatted_time, sizeof formatted_time, "%c", tm) != 0) {
                desc = formatted_time;
            }
        }

        if (desc != NULL) {
            new->desc = malloc(strlen(desc) + 1);
            if (new->desc != NULL) {
                strcpy(new->desc, desc);
            }
        }
    }

    return new;
}

static void free_save_state(struct save_state *s)
{
    if (s != NULL) {
        free(s->quetzal);
        free(s->desc);
        free(s);
    }
}

static void clear_save_stack(struct save_stack *s)
{
    while (s->head != NULL) {
        struct save_state *tmp = s->head;
        s->head = s->head->next;
        free_save_state(tmp);
    }
    s->tail = NULL;
    s->count = 0;
}

uint16_t variable(uint16_t var)
{
    ZASSERT(var < 0x100, "unable to decode variable %u", (unsigned)var);

    if (var == 0) { // Stack
        return pop_stack();
    } else if (var <= 0x0f) { // Locals
        ZASSERT(var <= CURRENT_FRAME->nlocals, "attempting to read from nonexistent local variable %d: routine has %d", (int)var, CURRENT_FRAME->nlocals);
        return CURRENT_FRAME->locals[var - 1];
    } else if (var <= 0xff) { // Globals
        var -= 0x10;
        return word(header.globals + (var * 2));
    }

    // This is an “impossible” situation (ie, the game did something wrong).
    // It will be caught above if safety checks are turned on, but if they
    // are not, do what we can: lie.
    return -1;
}

void store_variable(uint16_t var, uint16_t n)
{
    ZASSERT(var < 0x100, "unable to decode variable %u", (unsigned)var);

    if (var == 0) { // Stack
        push_stack(n);
    } else if (var <= 0x0f) { // Locals
        ZASSERT(var <= CURRENT_FRAME->nlocals, "attempting to store to nonexistent local variable %d: routine has %d", (int)var, CURRENT_FRAME->nlocals);
        CURRENT_FRAME->locals[var - 1] = n;
    } else if (var <= 0xff) { // Globals
        var -= 0x10;
        store_word(header.globals + (var * 2), n);
    }
}

uint16_t *stack_top_element(void)
{
    ZASSERT(sp > CURRENT_FRAME->sp, "stack underflow");

    return sp - 1;
}

void zpush(void)
{
    push_stack(zargs[0]);
}

void zpull(void)
{
    uint16_t v;

    if (zversion != 6) {
        v = pop_stack();

        // The z-spec 1.1 requires indirect variable references to the stack not to push/pop
        if (zargs[0] == 0) {
            *stack_top_element() = v;
        } else {
            store_variable(zargs[0], v);
        }
    } else {
        if (znargs == 0) {
            v = pop_stack();
        } else {
            uint16_t slots = user_word(zargs[0]) + 1;

            v = user_word(zargs[0] + (2 * slots));

            user_store_word(zargs[0], slots);
        }

        store(v);
    }
}

void zload(void)
{
    // The z-spec 1.1 requires indirect variable references to the stack not to push/pop
    if (zargs[0] == 0) {
        store(*stack_top_element());
    } else {
        store(variable(zargs[0]));
    }
}

void zstore(void)
{
    // The z-spec 1.1 requires indirect variable references to the stack not to push/pop
    if (zargs[0] == 0) {
        *stack_top_element() = zargs[1];
    } else {
        store_variable(zargs[0], zargs[1]);
    }
}

static void call(StoreWhere store_where)
{
    uint32_t jmp_to;
    uint8_t nlocals;
    uint16_t where;

    if (zargs[0] == 0) {
        // call(StoreWhere_Push) should never happen if zargs[0] is 0.
        if (store_where == StoreWhere_Variable) {
            store(0);
        }
        return;
    }

    jmp_to = unpack_routine(zargs[0]);
    ZASSERT(jmp_to < memory_size - 1, "call to invalid address 0x%lx", (unsigned long)jmp_to);

    nlocals = byte(jmp_to++);
    ZASSERT(nlocals <= 15, "too many (%d) locals at 0x%lx", nlocals, (unsigned long)jmp_to - 1);

    if (zversion <= 4) {
        ZASSERT(jmp_to + (nlocals * 2) < memory_size, "call to invalid address 0x%lx", (unsigned long)jmp_to);
    }

    switch (store_where) {
    case StoreWhere_Variable: where = byte(pc++); break; // Where to store return value
    case StoreWhere_None:     where = 0xff + 1;   break; // Or a tag meaning no return value
    case StoreWhere_Push:     where = 0xff + 2;   break; // Or a tag meaning push the return value
    default:                  die("internal error: invalid store_where value (%d)", (int)store_where);
    }

    add_frame(pc, sp, nlocals, znargs - 1, where);

    for (int i = 0; i < nlocals; i++) {
        if (i < znargs - 1) {
            CURRENT_FRAME->locals[i] = zargs[i + 1];
        } else {
            if (zversion <= 4) {
                CURRENT_FRAME->locals[i] = word(jmp_to + (2 * i));
            } else {
                CURRENT_FRAME->locals[i] = 0;
            }
        }
    }

    // Take care of locals!
    if (zversion <= 4) {
        jmp_to += nlocals * 2;
    }

    pc = jmp_to;
}

void start_v6(void)
{
    call(StoreWhere_None);
}

#ifdef ZTERP_GLK
uint16_t internal_call(uint16_t routine)
{
    uint16_t saved_args[znargs];
    uint16_t saved_nargs;

    memcpy(saved_args, zargs, sizeof saved_args);
    saved_nargs = znargs;

    znargs = 1;
    zargs[0] = routine;
    call(StoreWhere_Push);

    process_instructions();

    memcpy(zargs, saved_args, sizeof saved_args);
    znargs = saved_nargs;

    return pop_stack();
}
#endif

void zcall_store(void)
{
    call(StoreWhere_Variable);
}
void zcall_nostore(void)
{
    call(StoreWhere_None);
}

void do_return(uint16_t retval)
{
    uint16_t where;

    ZASSERT(NFRAMES > 1, "return attempted outside of a function");

    pc = CURRENT_FRAME->pc;
    sp = CURRENT_FRAME->sp;
    where = CURRENT_FRAME->where;
    fp--;

    if (where <= 0xff) {
        store_variable(where, retval);
    } else if (where == 0xff + 2) {
        push_stack(retval);
        interrupt_return();
    }
}

void zret_popped(void)
{
    do_return(pop_stack());
}

void zpop(void)
{
    pop_stack();
}

void zcatch(void)
{
    ZASSERT(zversion == 6 || NFRAMES > 1, "@catch called outside of a function");

    // Must account for the dummy frame in non-V6 stories.
    store(zversion == 6 ? NFRAMES : NFRAMES - 1);
}

void zthrow(void)
{
    // As with @catch, account for the dummy frame.
    if (zversion != 6) {
        zargs[1]++;
    }

    ZASSERT(zversion == 6 || NFRAMES > 1, "@throw called outside of a function");
    ZASSERT(zargs[1] <= NFRAMES, "unwinding too far");

    fp = BASE_OF_FRAMES + zargs[1];

    do_return(zargs[0]);
}

void zret(void)
{
    do_return(zargs[0]);
}

void zrtrue(void)
{
    do_return(1);
}

void zrfalse(void)
{
    do_return(0);
}

void zcheck_arg_count(void)
{
    branch_if(zargs[0] <= CURRENT_FRAME->nargs);
}

void zpop_stack(void)
{
    if (znargs == 1) {
        for (uint16_t i = 0; i < zargs[0]; i++) {
            pop_stack();
        }
    } else {
        user_store_word(zargs[1], user_word(zargs[1]) + zargs[0]);
    }
}

void zpush_stack(void)
{
    uint16_t slots = user_word(zargs[1]);

    if (slots == 0) {
        branch_if(false);
        return;
    }

    user_store_word(zargs[1] + (2 * slots), zargs[0]);
    user_store_word(zargs[1], slots - 1);

    branch_if(true);
}

// Compress dynamic memory according to Quetzal. Memory is allocated
// for the passed-in pointer, and must be freed by the caller. The
// return value is the size of compressed memory, or 0 on failure.
static uint32_t compress_memory(uint8_t **compressed)
{
    uint32_t ret = 0;
    long i = 0;
    uint8_t *tmp;

    // The output buffer needs to be 1.5× the size of dynamic memory for
    // the worst-case scenario: every other byte in memory differs from
    // the story file. This will cause every other byte to take up two
    // bytes in the output, thus creating 3 bytes of output for every 2 of
    // input. This should round up for the extreme case of alternating
    // zero/non-zero bytes with zeroes at the beginning and end, but due
    // to the fact that trailing zeroes are not stored, it does not need
    // to.
    tmp = malloc((3 * header.static_start) / 2);
    if (tmp == NULL) {
        return 0;
    }

    while (true) {
        long run = i;

        // Count zeroes. Stop counting when:
        // • The end of dynamic memory is reached, or
        // • A non-zero value is found
        while (i < header.static_start && (byte(i) ^ dynamic_memory[i]) == 0) {
            i++;
        }

        run = i - run;

        // A run of zeroes at the end need not be written.
        if (i == header.static_start) {
            break;
        }

        // If there has been a run of zeroes, write them out
        // 256 at a time.
        while (run > 0) {
            tmp[ret++] = 0;
            tmp[ret++] = (run > 256 ? 255 : run - 1);
            run -= 256;
        }

        // The current byte differs from the story, so write it.
        tmp[ret++] = byte(i) ^ dynamic_memory[i];

        i++;
    }

    // There is a small chance that ret will be 0 (if everything in
    // dynamic memory is identical to what was initially loaded from the
    // story). If that’s the case, this realloc() will instead behave as
    // free(), so just allocate one more than is necessary to avoid a
    // potential double free.
    *compressed = realloc(tmp, ret + 1);
    if (*compressed == NULL) {
        *compressed = tmp;
    }

    return ret;
}

// Reverse of the above function.
static bool uncompress_memory(const uint8_t *compressed, uint32_t size)
{
    uint32_t memory_index = 0;

    memcpy(memory, dynamic_memory, header.static_start);

    for (uint32_t i = 0; i < size; i++) {
        if (compressed[i] != 0) {
            if (memory_index == header.static_start) {
                return false;
            }
            store_byte(memory_index, byte(memory_index) ^ compressed[i]);
            memory_index++;
        } else {
            if (++i == size) {
                return false;
            }

            if (memory_index + (compressed[i] + 1) > header.static_start) {
                return false;
            }
            memory_index += (compressed[i] + 1);
        }
    }

    return true;
}

static TypeID write_ifhd(zterp_io *savefile, void *data)
{
    zterp_io_write16(savefile, header.release);
    zterp_io_write_exact(savefile, header.serial, 6);
    zterp_io_write16(savefile, header.checksum);
    zterp_io_write8(savefile, (pc >> 16) & 0xff);
    zterp_io_write8(savefile, (pc >>  8) & 0xff);
    zterp_io_write8(savefile, (pc >>  0) & 0xff);

    return &"IFhd";
}

// Store the filename in an IntD chunk.
static TypeID write_intd(zterp_io *savefile, void *data)
{
    zterp_io_write_exact(savefile, "UNIX", 4);
    zterp_io_write8(savefile, 0x02);
    zterp_io_write8(savefile, 0);
    zterp_io_write16(savefile, 0);
    zterp_io_write_exact(savefile, "    ", 4);
    zterp_io_write_exact(savefile, game_file, strlen(game_file));

    return &"IntD";
}

static TypeID write_mem(zterp_io *savefile, void *data)
{
    uint8_t *compressed = NULL;
    uint32_t memsize = compress_memory(&compressed);
    const uint8_t *mem = memory;
    TypeID type;

    // It is possible for the compressed memory size to be larger than
    // uncompressed; in this case, just store the uncompressed memory.
    if (memsize > 0 && memsize < header.static_start) {
        mem = compressed;
        type = &"CMem";
    } else {
        memsize = header.static_start;
        type = &"UMem";
    }
    zterp_io_write_exact(savefile, mem, memsize);
    free(compressed);

    return type;
}

// Quetzal save/restore functions.
static TypeID write_stks(zterp_io *savefile, void *data)
{
    // Add one more “fake” call frame with just enough information to
    // calculate the evaluation stack used by the current routine.
    fp->sp = sp;
    for (struct call_frame *p = BASE_OF_FRAMES; p != fp; p++) {
        uint8_t temp;

        zterp_io_write8(savefile, (p->pc >> 16) & 0xff);
        zterp_io_write8(savefile, (p->pc >>  8) & 0xff);
        zterp_io_write8(savefile, (p->pc >>  0) & 0xff);

        temp = p->nlocals;
        if (p->where > 0xff) {
            temp |= 0x10;
        }
        zterp_io_write8(savefile, temp);

        if (p->where > 0xff) {
            zterp_io_write8(savefile, 0);
        } else {
            zterp_io_write8(savefile, p->where);
        }

        zterp_io_write8(savefile, (1U << p->nargs) - 1);

        // number of words of evaluation stack used
        zterp_io_write16(savefile, (p + 1)->sp - p->sp);

        // local variables
        for (int i = 0; i < p->nlocals; i++) {
            zterp_io_write16(savefile, p->locals[i]);
        }

        // evaluation stack
        for (ptrdiff_t i = 0; i < (p + 1)->sp - p->sp; i++) {
            zterp_io_write16(savefile, p->sp[i]);
        }
    }

    return &"Stks";
}

static TypeID write_anno(zterp_io *savefile, void *data)
{
    char anno[32];

    snprintf(anno, sizeof anno, "Interpreter: Bocfel %s", ZTERP_VERSION);
    zterp_io_write_exact(savefile, anno, strlen(anno));

    return &"ANNO";
}

static TypeID write_args(zterp_io *savefile, void *data)
{
    enum SaveOpcode *saveopcode = data;

    zterp_io_write8(savefile, *saveopcode);

    for (int i = 0; i < znargs; i++) {
        zterp_io_write16(savefile, zargs[i]);
    }

    return &"Args";
}

static void write_undo_msav(zterp_io *savefile, enum SaveStackType type)
{
    struct save_stack *s = &save_stacks[type];

    zterp_io_write32(savefile, 0); // Version
    zterp_io_write32(savefile, s->count);

    for (const struct save_state *state = s->tail; state != NULL; state = state->prev) {
        if (type == SaveStackGame) {
            zterp_io_write8(savefile, state->savetype);
        } else if (type == SaveStackUser) {
            if (state->desc == NULL) {
                zterp_io_write32(savefile, 0);
            } else {
                zterp_io_write32(savefile, strlen(state->desc));
                zterp_io_write_exact(savefile, state->desc, strlen(state->desc));
            }
        }

        zterp_io_write32(savefile, state->quetzal_size);
        zterp_io_write_exact(savefile, state->quetzal, state->quetzal_size);
    }
}

static TypeID write_undo(zterp_io *savefile, void *data)
{
    write_undo_msav(savefile, SaveStackGame);

    return &"Undo";
}

static TypeID write_msav(zterp_io *savefile, void *data)
{
    write_undo_msav(savefile, SaveStackUser);

    return &"MSav";
}

static void write_chunk(zterp_io *io, TypeID (*writefunc)(zterp_io *savefile, void *data), void *data)
{
    long chunk_pos, end_pos, size;
    TypeID type;

    chunk_pos = zterp_io_tell(io);
    zterp_io_seek(io, 8, SEEK_CUR); // skip past type and size
    type = writefunc(io, data);
    if (type == NULL) {
        zterp_io_seek(io, chunk_pos, SEEK_SET);
        return;
    }
    end_pos = zterp_io_tell(io);
    size = end_pos - chunk_pos - 8;
    zterp_io_seek(io, chunk_pos, SEEK_SET);
    zterp_io_write_exact(io, *type, 4);
    zterp_io_write32(io, size);
    zterp_io_seek(io, end_pos, SEEK_SET);
    if (size & 1) {
        zterp_io_write8(io, 0); // padding
    }
}

// Meta saves (generated by the interpreter) are based on Quetzal. The
// format of the save state is the same (that is, the IFhd, IntD, and
// CMem/UMem chunks are identical). The type of the save file itself is
// BFZS instead of IFZS to prevent the files from being used by a normal
// @restore (as they are not compatible). See `Quetzal.md` for a
// description of how BFZS differs from IFZS.
static bool save_quetzal(zterp_io *savefile, enum SaveType savetype, enum SaveOpcode saveopcode, bool on_save_stack)
{
    long file_size;
    bool is_bfzs = savetype == SaveTypeMeta || savetype == SaveTypeAutosave;
    bool ok;

    zterp_io_try(savefile, ok);
    if (!ok) {
        return false;
    }

    zterp_io_write_exact(savefile, "FORM", 4);
    zterp_io_write32(savefile, 0); // to be filled in
    zterp_io_write_exact(savefile, is_bfzs ? "BFZS" : "IFZS", 4);

    write_chunk(savefile, write_ifhd, NULL);
    write_chunk(savefile, write_intd, NULL);
    write_chunk(savefile, write_mem, NULL);
    write_chunk(savefile, write_stks, NULL);
    write_chunk(savefile, write_anno, NULL);
    write_chunk(savefile, meta_write_bfnt, NULL);

    // When saving to a stack (either for undo or for in-memory saves),
    // don’t store history or persistent transcripts. History is
    // pointless, since the user can see this history already, and
    // persistent transcripts want to track what actually happened. If
    // the user types UNDO, for example, that should be reflected in the
    // transcript.
    if (on_save_stack) {
        write_chunk(savefile, screen_write_bfhs, NULL);
        write_chunk(savefile, screen_write_bfts, NULL);
    }

    // When restoring a meta save, @read will be called to bring the user
    // back to the same place as the save occurred. While memory and the
    // stack will be restored properly, the arguments to @read will not
    // (as the normal Z-machine save/restore mechanism doesn’t need them:
    // all restoring does is store or branch, using the location of the
    // program counter after restore). If this is a meta save, store zargs
    // so it can be restored before re-entering @read.
    if (is_bfzs) {
        write_chunk(savefile, write_args, &saveopcode);
        write_chunk(savefile, screen_write_scrn, NULL);
    }

    if (savetype == SaveTypeAutosave) {
        write_chunk(savefile, write_undo, NULL);
        write_chunk(savefile, write_msav, NULL);
        write_chunk(savefile, random_write_rand, NULL);
    }

    file_size = zterp_io_tell(savefile);
    zterp_io_seek(savefile, 4, SEEK_SET);
    zterp_io_write32(savefile, file_size - 8); // entire file size minus 8 (FORM + size)

    return true;
}

static void push_save_state(struct save_stack *save_stack, struct save_state *state)
{
    // If the maximum number has been reached, drop the last element.
    // A negative value for max_saves means there is no maximum.
    //
    // Small note: calling @restore_undo twice should succeed both times
    // if @save_undo was called twice (or three, four, etc. times). If
    // there aren’t enough slots, however, @restore_undo calls that should
    // work will fail because the earlier save states will have been
    // dropped. This can easily be seen by running TerpEtude’s undo test
    // with the slot count set to 1. By default, the number of save slots
    // is 100, so this will not be an issue unless a game goes out of its
    // way to cause problems.
    if (save_stack->max > 0 && save_stack->count == save_stack->max) {
        struct save_state *tmp = save_stack->tail;
        save_stack->tail = save_stack->tail->prev;
        if (save_stack->tail == NULL) {
            save_stack->head = NULL;
        } else {
            save_stack->tail->next = NULL;
        }
        free_save_state(tmp);
        save_stack->count--;
    }

    state->next = save_stack->head;
    state->prev = NULL;
    if (state->next != NULL) {
        state->next->prev = state;
    }
    save_stack->head = state;
    if (save_stack->tail == NULL) {
        save_stack->tail = state;
    }

    save_stack->count++;
}

#define goto_err(...)	do { show_message("Save file error: " __VA_ARGS__); goto err; } while (false)

static bool read_mem(zterp_iff *iff, zterp_io *savefile)
{
    uint32_t size;

    if (zterp_iff_find(iff, &"CMem", &size)) {
        uint8_t *buf;

        // Dynamic memory is 64KB, and a worst-case save should take up
        // 1.5× that value, or 96KB. Simply double the 64KB to avoid
        // potential edge-case problems.
        if (size > 131072) {
            goto_err("reported CMem size too large (%lu bytes)", (unsigned long)size);
        }

        buf = malloc(size * sizeof *buf);
        if (buf == NULL) {
            goto_err("unable to allocate memory for decompression");
        }

        if (!zterp_io_read_exact(savefile, buf, size)) {
            free(buf);
            goto_err("unexpected eof reading compressed memory");
        }

        if (!uncompress_memory(buf, size)) {
            free(buf);
            goto_err("memory cannot be uncompressed");
        }

        free(buf);
    } else if (zterp_iff_find(iff, &"UMem", &size)) {
        if (size != header.static_start) {
            goto_err("memory size mismatch");
        }
        if (!zterp_io_read_exact(savefile, memory, header.static_start)) {
            goto_err("unexpected eof reading memory");
        }
    } else {
        goto_err("no memory chunk found");
    }

    return true;

err:
    return false;
}

static bool read_stks(zterp_iff *iff, zterp_io *savefile)
{
    uint32_t size, n = 0, frameno = 0;

    if (!zterp_iff_find(iff, &"Stks", &size)) {
        goto_err("no stacks chunk found");
    }
    if (size == 0) {
        goto_err("empty stacks chunk");
    }

    sp = BASE_OF_STACK;
    fp = BASE_OF_FRAMES;

    while (n < size) {
        uint8_t frame[8];
        uint8_t nlocals;
        uint16_t nstack;
        uint8_t nargs = 0;
        uint32_t frame_pc;

        if (!zterp_io_read_exact(savefile, frame, sizeof frame)) {
            goto_err("unexpected eof reading stack frame");
        }
        n += sizeof frame;

        nlocals = frame[3] & 0xf;
        nstack = (frame[6] << 8) | frame[7];
        frame[5]++;
        while ((frame[5] >>= 1) != 0) {
            nargs++;
        }

        frame_pc = (((uint32_t)frame[0]) << 16) | (((uint32_t)frame[1]) << 8) | ((uint32_t)frame[2]);
        if (frame_pc >= memory_size) {
            goto_err("frame #%lu pc out of range (0x%lx)", (unsigned long)frameno, (unsigned long)frame_pc);
        }

        add_frame(frame_pc, sp, nlocals, nargs, (frame[3] & 0x10) ? 0xff + 1 : frame[4]);

        for (int i = 0; i < nlocals; i++) {
            uint16_t l;

            if (!zterp_io_read16(savefile, &l)) {
                goto_err("unexpected eof reading local variable");
            }
            CURRENT_FRAME->locals[i] = l;

            n += sizeof l;
        }

        for (uint16_t i = 0; i < nstack; i++) {
            uint16_t s;

            if (!zterp_io_read16(savefile, &s)) {
                goto_err("unexpected eof reading stack entry");
            }
            push_stack(s);

            n += sizeof s;
        }

        frameno++;
    }

    if (n != size) {
        goto_err("stack size mismatch");
    }

    return true;

err:
    return false;
}

static bool read_args(zterp_iff *iff, zterp_io *savefile, enum SaveOpcode *saveopcode)
{
    uint32_t size;
    uint8_t saveopcode_temp;

    if (!zterp_iff_find(iff, &"Args", &size)) {
        goto_err("no meta save Args chunk found");
    }

    if (!zterp_io_read8(savefile, &saveopcode_temp)) {
        goto_err("short read in Args");
    }

    *saveopcode = saveopcode_temp;

    size--;

    // @read takes between 1 and 4 operands, @read_char takes
    // between 1 and 3.
    switch (*saveopcode) {
    case SaveOpcodeRead:
        if (size != 2 && size != 4 && size != 6 && size != 8) {
            goto_err("invalid Args size: %lu", (unsigned long)size);
        }
        break;
    case SaveOpcodeReadChar:
        if (size != 2 && size != 4 && size != 6) {
            goto_err("invalid Args size: %lu", (unsigned long)size);
        }
        break;
    default:
        goto_err("invalid save opcode: %d\n", (int)*saveopcode);
    }

    znargs = size / 2;

    for (int i = 0; i < znargs; i++) {
        if (!zterp_io_read16(savefile, &zargs[i])) {
            goto_err("short read in Args");
        }
    }

    return true;

err:
    return false;
}

static bool read_bfzs_specific(zterp_iff *iff, zterp_io *savefile, enum SaveType savetype, enum SaveOpcode *saveopcode)
{
    uint32_t size;

    if (!read_args(iff, savefile, saveopcode)) {
        goto err;
    }

    if (savetype == SaveTypeAutosave && zterp_iff_find(iff, &"Rand", &size)) {
        random_read_rand(savefile);
    }

    if (zterp_iff_find(iff, &"Scrn", &size)) {
        char err[64];

        // Restoring cannot fail after this, because this function
        // actively touches the screen (changing window
        // configuration, etc). It would be possible to stash the
        // screen state, but it would (potentially) be lossy: if the
        // upper window is reduced, for example, there will be
        // missing text once it is enlarged again.
        if (!screen_read_scrn(savefile, size, err, sizeof err)) {
            goto_err("unable to parse screen state: %s", err);
        }
    } else {
        warning("no Scrn chunk in meta save");
    }

    return true;

err:
    return false;
}

// Any errors reading the Undo and MSav chunks are ignored. It’s better
// to have a valid autorestore with no undo/user saves than no
// autorestore at all.
static void read_undo_msav(zterp_io *savefile, uint32_t size, enum SaveStackType type)
{
    uint32_t version;
    uint32_t count;
    struct save_stack *save_stack = &save_stacks[type];
    size_t actual_size = 0;
    bool updated_seen_save_undo = seen_save_undo;

    if (!zterp_io_read32(savefile, &version) ||
        version != 0 ||
        !zterp_io_read32(savefile, &count)) {

        return;
    }

    actual_size += 4 + 4;

    clear_save_stack(save_stack);

    for (uint32_t i = 0; i < count; i++) {
        uint8_t savetype = SaveTypeMeta;
        char *desc = NULL;
        uint32_t quetzal_size;
        uint8_t *quetzal = NULL;
        struct save_state *new;

        if (type == SaveStackGame) {
            if (!zterp_io_read8(savefile, &savetype) ||
                (savetype != SaveTypeNormal && savetype != SaveTypeMeta)) {

                clear_save_stack(save_stack);
                return;
            }

            if (savetype == SaveTypeNormal) {
                updated_seen_save_undo = true;
            }

            actual_size += 1;
        } else if (type == SaveStackUser) {
            uint32_t desc_size;

            if (!zterp_io_read32(savefile, &desc_size) ||
                (desc = malloc(desc_size + 1)) == NULL ||
                !zterp_io_read_exact(savefile, desc, desc_size)) {

                clear_save_stack(save_stack);
                return;
            }

            desc[desc_size] = 0;

            actual_size += 4 + desc_size;
        }

        if (!zterp_io_read32(savefile, &quetzal_size) ||
            (quetzal = malloc(quetzal_size)) == NULL ||
            !zterp_io_read_exact(savefile, quetzal, quetzal_size) ||
            (new = new_save_state(savetype, desc)) == NULL) {

            free(desc);
            free(quetzal);
            clear_save_stack(save_stack);
            return;
        }

        free(desc);

        new->quetzal = quetzal;
        new->quetzal_size = quetzal_size;
        actual_size += 4 + quetzal_size;

        if (count - i > save_stack->max) {
            free_save_state(new);
        } else {
            push_save_state(save_stack, new);
        }
    }

    if (actual_size != size) {
        clear_save_stack(save_stack);
        return;
    }

    seen_save_undo = updated_seen_save_undo;
}

static void read_undo(zterp_io *savefile, uint32_t size)
{
    read_undo_msav(savefile, size, SaveStackGame);
}

static void read_msav(zterp_io *savefile, uint32_t size)
{
    read_undo_msav(savefile, size, SaveStackUser);
}

// Earlier versions of Bocfel’s meta-saves neglected to take into
// account the fact that the call to @read may have included arguments
// pulled from the stack. Any such arguments are lost in an old-style
// save, meaning the save file is not usable. However, most calls to
// @read do not use the stack, so most old-style saves should work just
// fine. This function is used to determine whether the @read
// instruction used for the meta-save pulled from the stack. If so, the
// restore will fail, but otherwise, it will proceed.
static bool instruction_has_stack_argument(uint32_t addr)
{
    uint32_t types = user_byte(addr++);

    for (int i = 6; i >= 0; i -= 2) {
        switch ((types >> i) & 0x03) {
        case 0:
            addr += 2;
            break;
        case 1:
            addr++;
            break;
        case 2:
            if (user_byte(addr++) == 0) {
                return true;
            }
            break;
        default:
            return false;
        }
    }

    return false;
}

static bool restore_quetzal(zterp_io *savefile, enum SaveType savetype, enum SaveOpcode *saveopcode)
{
    zterp_iff *iff;
    uint32_t size;
    uint8_t ifhd[13];
    uint32_t newpc;
    bool is_bfzs = savetype == SaveTypeMeta || savetype == SaveTypeAutosave;
    bool is_bfms = false;

    *saveopcode = SaveOpcodeNone;

    if (is_bfzs) {
        iff = zterp_iff_parse(savefile, &"BFZS");
        if (iff == NULL) {
            iff = zterp_iff_parse(savefile, &"BFMS");
            if (iff != NULL) {
                is_bfms = true;
            }
        }
    } else {
        iff = zterp_iff_parse(savefile, &"IFZS");
    }

    if (iff == NULL ||
        !zterp_iff_find(iff, &"IFhd", &size) ||
        size != 13 ||
        !zterp_io_read_exact(savefile, ifhd, sizeof ifhd)) {

        goto_err("corrupted save file or not a save file at all");
    }

    if (((ifhd[0] << 8) | ifhd[1]) != header.release ||
        memcmp(&ifhd[2], header.serial, sizeof header.serial) != 0 ||
        ((ifhd[8] << 8) | ifhd[9]) != header.checksum) {

        goto_err("wrong game or version");
    }

    newpc = (((uint32_t)ifhd[10]) << 16) | (((uint32_t)ifhd[11]) << 8) | ((uint32_t)ifhd[12]);
    if (newpc >= memory_size) {
        goto_err("pc out of range (0x%lx)", (unsigned long)newpc);
    }

    if (is_bfms && instruction_has_stack_argument(newpc + 1)) {
        goto_err("detected incompatible meta save: please file a bug report at https://bocfel.org/issues");
    }

    if (is_bfzs && savetype == SaveTypeAutosave) {
        if (zterp_iff_find(iff, &"Undo", &size)) {
            read_undo(savefile, size);
        }

        if (zterp_iff_find(iff, &"MSav", &size)) {
            read_msav(savefile, size);
        }
    }

    stash_backup();

    if (!read_mem(iff, savefile) || !read_stks(iff, savefile)) {
        goto err;
    }

    if (zterp_iff_find(iff, &"Bfnt", &size)) {
        meta_read_bfnt(savefile, size);
    }

    if (zterp_iff_find(iff, &"Bfhs", &size)) {
        if (savetype == SaveTypeAutosave || !options.disable_history_playback) {
            long start = zterp_io_tell(savefile);

            screen_read_bfhs(savefile, savetype == SaveTypeAutosave);

            if (zterp_io_tell(savefile) - start != size) {
                goto_err("history size mismatch");
            }
        }
    } else if (savetype == SaveTypeAutosave) {
        warning("unable to find history record");
        screen_printf(">");
    }

    if (zterp_iff_find(iff, &"Bfts", &size)) {
        screen_read_bfts(savefile, size);
    }

    if (is_bfzs && !is_bfms) {
        // This must be the last restore call, since it can ultimately
        // wind up modifying the screen state in an irreversible way.
        if (!read_bfzs_specific(iff, savefile, savetype, saveopcode)) {
            goto err;
        }
    }

    stash_free();
    zterp_iff_free(iff);

    pc = newpc;

    return true;

err:
    // If a stash exists, then something vital has been scribbled upon.
    // Try to restore the stash, but if it can’t be restored, the only
    // course of action is to exit.
    if (stash_exists() && !stash_restore()) {
        die("the system is likely in an inconsistent state");
    }

    // If an autosave fails, clear the save stacks: autosaves can
    // contain save data that might have been loaded already. These
    // could use the stash mechanism, but since autosaves are loaded
    // before anything happens, it is guaranteed that the stacks were
    // empty before the restore process started. This is faster and
    // simpler than stashing.
    if (is_bfzs && savetype == SaveTypeAutosave) {
        clear_save_stack(&save_stacks[SaveStackGame]);
        clear_save_stack(&save_stacks[SaveStackUser]);
    }

    stash_free();
    zterp_iff_free(iff);

    return false;
}

#undef goto_err

static bool open_savefile(enum SaveType savetype, enum zterp_io_mode mode, zterp_io **savefile)
{
    char autosave_name[ZTERP_OS_PATH_SIZE];
    const char *filename = NULL;

    if (savetype == SaveTypeAutosave) {
        if (!zterp_os_autosave_name(&autosave_name)) {
            return false;
        }

        filename = autosave_name;
    }

    *savefile = zterp_io_open(filename, mode, ZTERP_IO_PURPOSE_SAVE);
    if (*savefile == NULL) {
        if (savetype != SaveTypeAutosave) {
            warning("unable to open save file");
        }

        return false;
    }

    return true;
}

// Perform all aspects of a save, apart from storing/branching.
// Returns true if the save was success, false if not.
bool do_save(enum SaveType savetype, enum SaveOpcode saveopcode)
{
    zterp_io *savefile;
    bool success;

    if (!open_savefile(savetype, ZTERP_IO_MODE_WRONLY, &savefile)) {
        return false;
    }

    success = save_quetzal(savefile, savetype, saveopcode, true);

    zterp_io_close(savefile);

    return success;
}

// The suggested filename is ignored, because Glk and, at least as of
// right now, zterp_io_open(), do not provide a method to do this.
// The “prompt” argument added by standard 1.1 is thus also ignored.
void zsave(void)
{
    if (in_interrupt()) {
        die("@save called inside of an interrupt");
    }

    bool success = do_save(SaveTypeNormal, SaveOpcodeNone);

    if (zversion <= 3) {
        branch_if(success);
    } else {
        store(success ? 1 : 0);
    }
}

// Perform all aspects of a restore, apart from storing/branching.
// Returns true if the restore was success, false if not.
// *saveopcode will be set appropriately based on the type of save file
// this is.
bool do_restore(enum SaveType savetype, enum SaveOpcode *saveopcode)
{
    zterp_io *savefile;
    uint16_t flags2;
    bool success;

    if (!open_savefile(savetype, ZTERP_IO_MODE_RDONLY, &savefile)) {
        return false;
    }

    flags2 = word(0x10);

    success = restore_quetzal(savefile, savetype, saveopcode);

    zterp_io_close(savefile);

    if (success) {
        // §8.6.1.3
        if (zversion == 3) {
            close_upper_window();
        }

        // §6.1.2.2: The save might be from a different interpreter with
        // different capabilities, so update the header to indicate what the
        // current capabilities are...
        write_header();

        // ...except that flags2 should be preserved (§6.1.2).
        store_word(0x10, flags2);

        // Redraw the status line in games that use one.
        if (zversion <= 3) {
            zshow_status();
        }
    }

    return success;
}

void zrestore(void)
{
    enum SaveOpcode saveopcode;
    bool success = do_restore(SaveTypeNormal, &saveopcode);

    if (zversion <= 3) {
        branch_if(success);
    } else {
        store(success ? 2 : 0);
    }

    // On a successful restore, we are outside of any interrupt (since
    // @save cannot be called inside an interrupt), so reset the level
    // back to zero.
    if (success) {
        interrupt_restore(saveopcode);
    }
}

// Push the current state onto the specified save stack.
enum SaveResult push_save(enum SaveStackType type, enum SaveType savetype, enum SaveOpcode saveopcode, const char *desc)
{
    struct save_stack *s = &save_stacks[type];
    struct save_state *new;
    zterp_io *savefile = NULL;

    if (s->max == 0) {
        return SaveResultUnavailable;
    }

    new = new_save_state(savetype, desc);
    if (new == NULL) {
        return SaveResultFailure;
    }

    savefile = zterp_io_open_memory(NULL, 0, ZTERP_IO_MODE_WRONLY);
    if (savefile == NULL) {
        goto err;
    }

    if (!save_quetzal(savefile, savetype, saveopcode, false)) {
        goto err;
    }

    if (!zterp_io_close_memory(savefile, &new->quetzal, &new->quetzal_size)) {
        goto err;
    }

    push_save_state(s, new);

    return SaveResultSuccess;

err:
    if (savefile != NULL) {
        zterp_io_close(savefile);
    }
    free_save_state(new);

    return SaveResultFailure;
}

// Remove the first “n” saves from the specified stack. If there are
// not enough saves available, remove all saves.
static void trim_saves(enum SaveStackType type, long n)
{
    struct save_stack *s = &save_stacks[type];

    while (n-- > 0 && s->count > 0) {
        struct save_state *tmp = s->head;

        s->head = s->head->next;
        s->count--;
        free_save_state(tmp);
        if (s->head != NULL) {
            s->head->prev = NULL;
        }
        if (s->count == 0) {
            s->tail = NULL;
        }
    }
}

// Pop the specified state from the specified stack and jump to it.
// Indices start at 0, with 0 being the most recent save.
bool pop_save(enum SaveStackType type, long saveno, enum SaveOpcode *saveopcode)
{
    struct save_stack *s = &save_stacks[type];
    struct save_state *p = s->head;
    uint16_t flags2;
    zterp_io *savefile;

    if (saveno >= s->count) {
        return false;
    }

    flags2 = word(0x10);

    for (long i = 0; i < saveno; i++) {
        p = p->next;
    }

    savefile = zterp_io_open_memory(p->quetzal, p->quetzal_size, ZTERP_IO_MODE_RDONLY);
    if (savefile == NULL) {
        return false;
    }

    if (!restore_quetzal(savefile, p->savetype, saveopcode)) {
        zterp_io_close(savefile);
        return false;
    }

    zterp_io_close(savefile);

    trim_saves(type, saveno + 1);

    // §6.1.2.2: As with @restore, header values marked with Rst (in
    // §11) should be reset. Unlike with @restore, it can be assumed
    // that the game was saved by the same interpreter, but it cannot be
    // assumed that the screen size is the same as it was at the time
    // @save_undo was called.
    write_header();

    // §6.1.2: Flags 2 should be preserved.
    store_word(0x10, flags2);

    return true;
}

// Wrapper around trim_saves which reports failure if the specified save
// does not exist.
bool drop_save(enum SaveStackType type, long i)
{
    struct save_stack *s = &save_stacks[type];

    if (i > s->count) {
        return false;
    }

    trim_saves(type, i);

    return true;
}

void list_saves(enum SaveStackType type, void (*printer)(const char *))
{
    struct save_stack *s = &save_stacks[type];
    long nsaves = s->count;

    if (nsaves == 0) {
        printer("[No saves available]");
        return;
    }

    for (struct save_state *p = s->tail; p != NULL; p = p->prev) {
        char buf[4096];

        if (p->desc != NULL) {
            snprintf(buf, sizeof buf, "%ld. %s%s", nsaves--, p->desc, p->prev == NULL ? " *" : "");
        } else {
            snprintf(buf, sizeof buf, "<no description>");
        }
        printer(buf);
    }
}

void zsave_undo(void)
{
    if (in_interrupt()) {
        die("@save_undo called inside of an interrupt");
    }

    // If override undo is set, all calls to @save_undo are reported as
    // failure; the interpreter still reports that @save_undo is
    // available, however. These values will be tuned if it becomes
    // necessary (i.e. if some games behave badly).
    if (options.override_undo) {
        store(0);
    } else {
        // On the first call to @save_undo, switch over to game-based save
        // states instead of interpreter-generated save states.
        if (!seen_save_undo) {
            clear_save_stack(&save_stacks[SaveStackGame]);
            seen_save_undo = true;
        }

        switch (push_save(SaveStackGame, SaveTypeNormal, SaveOpcodeNone, NULL)) {
        case SaveResultSuccess:
            store(1);
            break;
        case SaveResultFailure:
            store(0);
            break;
        // @save_undo must return -1 if undo is not available. The only
        // time that’s the case with Bocfel is if the user has requested
        // 0 save slots. Otherwise, @save_undo might still fail as a
        // result of low memory, but that’s a transient failure for
        // which 0 will be returned.
        case SaveResultUnavailable:
            store(0xffff);
            break;
        }
    }
}

void zrestore_undo(void)
{
    // If @save_undo has not been called, @restore_undo should fail, even
    // if there are interpreter-generated save states available.
    if (!seen_save_undo) {
        store(0);
    } else {
        enum SaveOpcode saveopcode;
        bool success = pop_save(SaveStackGame, 0, &saveopcode);

        store(success ? 2 : 0);

        if (success) {
            interrupt_restore(saveopcode);
        }
    }
}

static uint16_t zargs_stash[8];
static int znargs_stash;

static void args_stash_backup(void)
{
    memcpy(zargs_stash, zargs, znargs);
    znargs_stash = znargs;
}

static bool args_stash_restore(void)
{
    memcpy(zargs, zargs_stash, znargs_stash);
    znargs = znargs_stash;

    return true;
}

static void args_stash_free(void)
{
}

static uint8_t *memory_backup;

static void memory_stash_backup(void)
{
    memory_backup = malloc(header.static_start);
    if (memory_backup != NULL) {
        memcpy(memory_backup, memory, header.static_start);
    }
}

static bool memory_stash_restore(void)
{
    if (memory_backup == NULL) {
        return false;
    }

    memcpy(memory, memory_backup, header.static_start);

    return true;
}

static void memory_stash_free(void)
{
    free(memory_backup);
    memory_backup = NULL;
}

static uint16_t *stack_backup;
static long stack_backup_size;

static void stack_stash_backup(void)
{
    stack_backup_size = sp - stack;
    if (stack_backup_size != 0) {
        stack_backup = malloc(stack_backup_size * sizeof *stack);
        if (stack_backup != NULL) {
            memcpy(stack_backup, stack, stack_backup_size * sizeof *stack);
        }
    }
}

static bool stack_stash_restore(void)
{
    if (stack_backup == NULL && stack_backup_size != 0) {
        return false;
    }

    sp = BASE_OF_STACK + stack_backup_size;
    if (stack_backup_size != 0) {
        memcpy(stack, stack_backup, stack_backup_size * sizeof *stack);
    }

    return true;
}

static void stack_stash_free(void)
{
    free(stack_backup);
    stack_backup = NULL;
}

static struct call_frame *frames_backup;
static long frames_backup_size;

static void frames_stash_backup(void)
{
    frames_backup_size = NFRAMES;
    if (frames_backup_size != 0) {
        frames_backup = malloc(frames_backup_size * sizeof *frames);
        if (frames_backup != NULL) {
            memcpy(frames_backup, frames, frames_backup_size * sizeof *frames);
        }
    }
}

static bool frames_stash_restore(void)
{
    if (frames_backup == NULL && frames_backup_size != 0) {
        return false;
    }

    fp = BASE_OF_FRAMES + frames_backup_size;
    if (frames_backup_size != 0) {
        memcpy(frames, frames_backup, frames_backup_size * sizeof *frames);
    }

    return true;
}

static void frames_stash_free(void)
{
    free(frames_backup);
    frames_backup = NULL;
}

void init_stack(bool first_run)
{
    // Allocate space for the evaluation and call stacks.
    // Clamp the size between 1 and the largest value that will not
    // produce an overflow of size_t when multiplied by the size of the
    // type.
    // Also, the call stack can be no larger than 0xffff so that the
    // result of a @catch will fit into a 16-bit integer.
    if (first_run) {
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define CLAMP(n, a, b)	((n) < (a) ? (a) : (n) > (b) ? (b): (n))
        options.eval_stack_size = CLAMP(options.eval_stack_size, 1, SIZE_MAX / sizeof *stack);
        stack = malloc(options.eval_stack_size * sizeof *stack);
        if (stack == NULL) {
            die("unable to allocate %lu bytes for the evaluation stack", options.eval_stack_size * (unsigned long)sizeof *stack);
        }
        TOP_OF_STACK = &stack[options.eval_stack_size];

        options.call_stack_size = CLAMP(options.call_stack_size, 1, MIN(0xffff, (SIZE_MAX / sizeof *frames) - sizeof *frames));
        // One extra to help with saving (thus the subtraction of sizeof *frames above).
        frames = malloc((options.call_stack_size + 1) * sizeof *frames);
        if (frames == NULL) {
            die("unable to allocate %lu bytes for the call stack", (options.call_stack_size + 1) * (unsigned long)sizeof *frames);
        }
        TOP_OF_FRAMES = &frames[options.call_stack_size];
#undef MIN
#undef CLAMP

        stash_register(args_stash_backup, args_stash_restore, args_stash_free);
        stash_register(memory_stash_backup, memory_stash_restore, memory_stash_free);
        stash_register(stack_stash_backup, stack_stash_restore, stack_stash_free);
        stash_register(frames_stash_backup, frames_stash_restore, frames_stash_free);
    }

    sp = BASE_OF_STACK;
    fp = BASE_OF_FRAMES;

    // Quetzal requires a dummy frame in non-V6 games, so do that here.
    if (zversion != 6) {
        add_frame(0, sp, 0, 0, 0);
    }

    // Free all @save_undo save states.
    clear_save_stack(&save_stacks[SaveStackGame]);
    save_stacks[SaveStackGame].max = options.max_saves;
    seen_save_undo = false;

    // Free all /ps save states.
    clear_save_stack(&save_stacks[SaveStackUser]);
    save_stacks[SaveStackUser].max = 25;
}
