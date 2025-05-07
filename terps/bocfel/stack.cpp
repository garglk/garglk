// Copyright 2010-2021 Chris Spiegel.
//
// SPDX-License-Identifier: MIT

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <iomanip>
#include <locale>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "stack.h"
#include "branch.h"
#include "iff.h"
#include "io.h"
#include "memory.h"
#include "meta.h"
#include "options.h"
#include "osdep.h"
#include "process.h"
#include "random.h"
#include "screen.h"
#include "stash.h"
#include "types.h"
#include "util.h"
#include "zterp.h"

using namespace std::literals;

enum class StoreWhere {
    Variable,
    None,
    Push,
};

struct CallFrame {
    uint32_t pc;
    uint16_t *sp;
    uint8_t nlocals;
    uint8_t nargs;
    uint16_t where;
    std::array<uint16_t, 15> locals;
};

static CallFrame *frames;
static CallFrame *fp;

#define BASE_OF_FRAMES	frames
static CallFrame *TOP_OF_FRAMES;
#define CURRENT_FRAME	(fp - 1)
#define NFRAMES		(static_cast<long>(fp - frames))

static uint16_t *stack;
static uint16_t *sp;

#define BASE_OF_STACK	stack
static uint16_t *TOP_OF_STACK;

static void push_stack(uint16_t n)
{
    ZASSERT(sp != TOP_OF_STACK, "stack overflow");
    *sp++ = n;
}

static uint16_t pop_stack()
{
    ZASSERT(sp > CURRENT_FRAME->sp, "stack underflow");
    return *--sp;
}

struct SaveState {
public:
    SaveType savetype;
    std::vector<uint8_t> quetzal;
    std::string desc;

    SaveState(SaveType savetype_, const char *desc_, std::vector<uint8_t> quetzal_) :
        savetype(savetype_),
        quetzal(std::move(quetzal_)),
        desc(desc_ == nullptr ? format_time() : desc_)
    {
    }

private:
    static std::string format_time() {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto lt = std::localtime(&now);

        if (lt == nullptr) {
            return "<no time information>";
        } else {
            std::ostringstream formatted_time;

            try {
                formatted_time.imbue(std::locale(""));
            } catch (...) {
                // If the locale is invalid, don’t worry about it.
            }

            formatted_time << std::put_time(lt, "%c");

            return formatted_time.str();
        }
    }
};

struct SaveStack {
    std::deque<SaveState> states;
    unsigned long max = 0;

    void push(SaveState state) {
        // If the maximum number has been reached, drop the last element.
        //
        // Small note: calling @restore_undo twice should succeed both times
        // if @save_undo was called twice (or three, four, etc. times). If
        // there aren’t enough slots, however, @restore_undo calls that should
        // work will fail because the earlier save states will have been
        // dropped. This can easily be seen by running TerpEtude’s undo test
        // with the slot count set to 1. By default, the number of save slots
        // is 100, so this will not be an issue unless a game goes out of its
        // way to cause problems.
        if (max > 0 && states.size() == max) {
            states.pop_back();
        }

        states.push_front(std::move(state));
    }

    // Remove the first “n” saves from the specified stack. If there are
    // not enough saves available, remove all saves.
    void trim_saves(size_t n) {
        states.erase(states.begin(), n > states.size() ? states.end() : states.begin() + n);
        states.shrink_to_fit();
    }

    void clear() {
        states.clear();
        states.shrink_to_fit();
    }
};
static std::unordered_map<SaveStackType, SaveStack, EnumClassHash> save_stacks;

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

uint16_t variable(uint16_t var)
{
    ZASSERT(var < 0x100, "unable to decode variable %u", static_cast<unsigned int>(var));

    if (var == 0) { // Stack
        return pop_stack();
    } else if (var <= 0x0f) { // Locals
        ZASSERT(var <= CURRENT_FRAME->nlocals, "attempting to read from nonexistent local variable %d: routine has %d", static_cast<int>(var), CURRENT_FRAME->nlocals);
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
    ZASSERT(var < 0x100, "unable to decode variable %u", static_cast<unsigned int>(var));

    if (var == 0) { // Stack
        push_stack(n);
    } else if (var <= 0x0f) { // Locals
        ZASSERT(var <= CURRENT_FRAME->nlocals, "attempting to store to nonexistent local variable %d: routine has %d", static_cast<int>(var), CURRENT_FRAME->nlocals);
        CURRENT_FRAME->locals[var - 1] = n;
    } else if (var <= 0xff) { // Globals
        var -= 0x10;
        store_word(header.globals + (var * 2), n);
    }
}

uint16_t *stack_top_element()
{
    ZASSERT(sp > CURRENT_FRAME->sp, "stack underflow");

    return sp - 1;
}

void zpush()
{
    push_stack(zargs[0]);
}

void zpull()
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

void zload()
{
    // The z-spec 1.1 requires indirect variable references to the stack not to push/pop
    if (zargs[0] == 0) {
        store(*stack_top_element());
    } else {
        store(variable(zargs[0]));
    }
}

void zstore()
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
        // call(StoreWhere::Push) should never happen if zargs[0] is 0.
        if (store_where == StoreWhere::Variable) {
            store(0);
        }
        return;
    }

    jmp_to = unpack_routine(zargs[0]);
    ZASSERT(jmp_to < memory_size - 1, "call to invalid address 0x%lx", static_cast<unsigned long>(jmp_to));

    nlocals = byte(jmp_to++);
    ZASSERT(nlocals <= 15, "too many (%d) locals at 0x%lx", nlocals, static_cast<unsigned long>(jmp_to) - 1);

    if (zversion <= 4) {
        ZASSERT(jmp_to + (nlocals * 2) < memory_size, "call to invalid address 0x%lx", static_cast<unsigned long>(jmp_to));
    }

    switch (store_where) {
    case StoreWhere::Variable: where = byte(pc++); break; // Where to store return value
    case StoreWhere::None:     where = 0xff + 1;   break; // Or a tag meaning no return value
    case StoreWhere::Push:     where = 0xff + 2;   break; // Or a tag meaning push the return value
    default:                   die("internal error: invalid store_where value (%d)", static_cast<int>(store_where));
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

void start_v6()
{
    call(StoreWhere::None);
}

uint16_t internal_call(uint16_t routine, std::vector<uint16_t> args)
{
    std::vector<uint16_t> saved_args(zargs.begin(), zargs.begin() + znargs);

    ZASSERT(args.size() < 8, "internal error: too many arguments");

    znargs = 1 + args.size();
    zargs[0] = routine;
    std::copy(args.begin(), args.end(), &zargs[1]);
    call(StoreWhere::Push);

    process_instructions();

    std::copy(saved_args.begin(), saved_args.end(), zargs.begin());
    znargs = saved_args.size();

    return pop_stack();
}

void zcall_store()
{
    call(StoreWhere::Variable);
}
void zcall_nostore()
{
    call(StoreWhere::None);
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
        throw Operation::Return();
    }
}

void zret_popped()
{
    do_return(pop_stack());
}

void zpop()
{
    pop_stack();
}

void zcatch()
{
    ZASSERT(zversion == 6 || NFRAMES > 1, "@catch called outside of a function");

    // Must account for the dummy frame in non-V6 stories.
    store(zversion == 6 ? NFRAMES : NFRAMES - 1);
}

void zthrow()
{
    // As with @catch, account for the dummy frame.
    if (zversion != 6) {
        ZASSERT(zargs[1] != 0xffff, "unwinding too far");
        zargs[1]++;
    }

    ZASSERT(zversion == 6 || NFRAMES > 1, "@throw called outside of a function");
    ZASSERT(zargs[1] <= NFRAMES, "unwinding too far");

    fp = BASE_OF_FRAMES + zargs[1];

    do_return(zargs[0]);
}

void zret()
{
    do_return(zargs[0]);
}

void zrtrue()
{
    do_return(1);
}

void zrfalse()
{
    do_return(0);
}

void zcheck_arg_count()
{
    branch_if(zargs[0] <= CURRENT_FRAME->nargs);
}

void zpop_stack()
{
    if (znargs == 1) {
        for (uint16_t i = 0; i < zargs[0]; i++) {
            pop_stack();
        }
    } else {
        user_store_word(zargs[1], user_word(zargs[1]) + zargs[0]);
    }
}

void zpush_stack()
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

// Compress dynamic memory according to Quetzal. On failure,
// std::bad_alloc is thrown.
static std::vector<uint8_t> compress_memory()
{
    long i = 0;
    std::vector<uint8_t> compressed;

    compressed.reserve(header.static_start);

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
            compressed.push_back(0);
            compressed.push_back(run > 256 ? 255 : run - 1);
            run -= 256;
        }

        // The current byte differs from the story, so write it.
        compressed.push_back(byte(i) ^ dynamic_memory[i]);

        i++;
    }

    return compressed;
}

// Reverse of the above function.
static bool uncompress_memory(const uint8_t *compressed, uint32_t size)
{
    uint32_t memory_index = 0;

    std::memcpy(memory, dynamic_memory, header.static_start);

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

static IFF::TypeID write_ifhd(IO &savefile)
{
    savefile.write16(header.release);
    savefile.write_exact(header.serial, sizeof header.serial);
    savefile.write16(header.checksum);
    savefile.write8((pc >> 16) & 0xff);
    savefile.write8((pc >>  8) & 0xff);
    savefile.write8((pc >>  0) & 0xff);

    return IFF::TypeID("IFhd");
}

// Store the filename in an IntD chunk.
static IFF::TypeID write_intd(IO &savefile)
{
    savefile.write_exact("UNIX", 4);
    savefile.write8(0x02);
    savefile.write8(0);
    savefile.write16(0);
    savefile.write_exact("    ", 4);
    savefile.write_exact(game_file.c_str(), game_file.size());

    return IFF::TypeID("IntD");
}

static IFF::TypeID write_mem(IO &savefile)
{
    std::vector<uint8_t> compressed;
    uint32_t memsize = header.static_start;
    const uint8_t *mem = memory;
    IFF::TypeID type = IFF::TypeID("UMem");

    try {
        compressed = compress_memory();
        // It is possible for the compressed memory size to be larger than
        // uncompressed; in this case, don’t use compressed memory.
        if (compressed.size() < header.static_start) {
            mem = compressed.data();
            memsize = compressed.size();
            type = IFF::TypeID("CMem");
        }
    } catch (const std::bad_alloc &) {
    }

    savefile.write_exact(mem, memsize);

    return type;
}

// Quetzal save/restore functions.
static IFF::TypeID write_stks(IO &savefile)
{
    // Add one more “fake” call frame with just enough information to
    // calculate the evaluation stack used by the current routine.
    fp->sp = sp;
    for (CallFrame *p = BASE_OF_FRAMES; p != fp; p++) {
        uint8_t flags;

        savefile.write8((p->pc >> 16) & 0xff);
        savefile.write8((p->pc >>  8) & 0xff);
        savefile.write8((p->pc >>  0) & 0xff);

        flags = p->nlocals;
        if (p->where > 0xff) {
            flags |= 0x10;
        }
        savefile.write8(flags);

        if (p->where > 0xff) {
            savefile.write8(0);
        } else {
            savefile.write8(p->where);
        }

        savefile.write8((1U << p->nargs) - 1);

        // number of words of evaluation stack used
        savefile.write16((p + 1)->sp - p->sp);

        // local variables
        for (int i = 0; i < p->nlocals; i++) {
            savefile.write16(p->locals[i]);
        }

        // evaluation stack
        for (std::ptrdiff_t i = 0; i < (p + 1)->sp - p->sp; i++) {
            savefile.write16(p->sp[i]);
        }
    }

    return IFF::TypeID("Stks");
}

static IFF::TypeID write_anno(IO &savefile)
{
    std::string anno = "Interpreter: Bocfel "s + ZTERP_VERSION;
    savefile.write_exact(anno.c_str(), anno.size());

    return IFF::TypeID("ANNO");
}

static IFF::TypeID write_args(IO &savefile, SaveOpcode saveopcode)
{
    savefile.write8(static_cast<uint8_t>(saveopcode));

    for (int i = 0; i < znargs; i++) {
        savefile.write16(zargs[i]);
    }

    return IFF::TypeID("Args");
}

static void write_undo_msav(IO &savefile, SaveStackType type)
{
    SaveStack &s = save_stacks[type];

    savefile.write32(0); // Version
    savefile.write32(s.states.size());

    for (auto state = s.states.crbegin(); state != s.states.crend(); ++state) {
        if (type == SaveStackType::Game) {
            savefile.write8(static_cast<uint8_t>(state->savetype));
        } else if (type == SaveStackType::User) {
            if (state->desc.empty()) {
                savefile.write32(0);
            } else {
                savefile.write32(state->desc.size());
                savefile.write_exact(state->desc.c_str(), state->desc.size());
            }
        }

        savefile.write32(state->quetzal.size());
        savefile.write_exact(state->quetzal.data(), state->quetzal.size());
    }
}

static IFF::TypeID write_undo(IO &savefile)
{
    write_undo_msav(savefile, SaveStackType::Game);

    return IFF::TypeID("Undo");
}

static IFF::TypeID write_msav(IO &savefile)
{
    write_undo_msav(savefile, SaveStackType::User);

    return IFF::TypeID("MSav");
}

template<typename... Types>
static void write_chunk(IO &io, IFF::TypeID (*writefunc)(IO &savefile, Types... args), Types... args)
{
    long chunk_pos, end_pos, size;
    IFF::TypeID type;

    chunk_pos = io.tell();
    // Type and size, to be filled in below.
    io.write32(0);
    io.write32(0);
    type = writefunc(io, args...);
    if (type.empty()) {
        io.seek(chunk_pos, IO::SeekFrom::Start);
        return;
    }
    end_pos = io.tell();
    size = end_pos - chunk_pos - 8;
    io.seek(chunk_pos, IO::SeekFrom::Start);
    io.write32(type.val());
    io.write32(size);
    io.seek(end_pos, IO::SeekFrom::Start);
    if ((size & 1) == 1) {
        io.write8(0); // padding
    }
}

// Meta saves (generated by the interpreter) are based on Quetzal. The
// format of the save state is the same (that is, the IFhd, IntD, and
// CMem/UMem chunks are identical). The type of the save file itself is
// BFZS instead of IFZS to prevent the files from being used by a normal
// @restore (as they are not compatible). See Quetzal.md for a
// description of how BFZS differs from IFZS.
static bool save_quetzal(IO &savefile, SaveType savetype, SaveOpcode saveopcode, bool on_save_stack)
{
    try {
        long file_size;
        bool is_bfzs = savetype == SaveType::Meta || savetype == SaveType::Autosave;

        savefile.write_exact("FORM", 4);
        savefile.write32(0); // to be filled in
        savefile.write_exact(is_bfzs ? "BFZS" : "IFZS", 4);

        write_chunk(savefile, write_ifhd);
        write_chunk(savefile, write_intd);
        write_chunk(savefile, write_mem);
        write_chunk(savefile, write_stks);
        write_chunk(savefile, write_anno);

        // When saving to a stack (either for undo or for in-memory saves),
        // don’t store history, persistent transcripts, or notes. History is
        // pointless, since the user can see this history already. Persistent
        // transcripts want to track what actually happened. If the user types
        // UNDO, for example, that should be reflected in the transcript. And
        // notes shouldn’t be cleared just because the user undid a turn; if a
        // user doesn’t want the notes, he can delete them, but he can’t bring
        // back notes that he never wanted deleted.
        if (!on_save_stack) {
            write_chunk(savefile, screen_write_bfhs);
            write_chunk(savefile, screen_write_bfts);
            write_chunk(savefile, meta_write_bfnt);
        }

        // When restoring a meta save, @read will be called to bring the user
        // back to the same place as the save occurred. While memory and the
        // stack will be restored properly, the arguments to @read will not
        // (as the normal Z-machine save/restore mechanism doesn’t need them:
        // all restoring does is store or branch, using the location of the
        // program counter after restore). If this is a meta save, store zargs
        // so it can be restored before re-entering @read.
        if (is_bfzs) {
            write_chunk(savefile, write_args, saveopcode);
            write_chunk(savefile, screen_write_scrn);
        }

        if (savetype == SaveType::Autosave) {
            write_chunk(savefile, write_undo);
            write_chunk(savefile, write_msav);
            write_chunk(savefile, random_write_rand);
        }

        file_size = savefile.tell();
        savefile.seek(4, IO::SeekFrom::Start);
        savefile.write32(file_size - 8); // entire file size minus 8 (FORM + size)

        return true;
    } catch (const IO::IOError &) {
        return false;
    }
}

static void read_mem(IFF &iff)
{
    uint32_t size;

    if (iff.find(IFF::TypeID("CMem"), size)) {
        std::vector<uint8_t> buf;

        // Dynamic memory is 64KB, and a worst-case save should take up
        // 1.5× that value, or 96KB. Simply double the 64KB to avoid
        // potential edge-case problems.
        if (size > 131072) {
            throw RestoreError(fstring("reported CMem size too large (%lu bytes)", static_cast<unsigned long>(size)));
        }

        if (size > 0) {
            try {
                buf.resize(size);
                iff.io()->read_exact(buf.data(), size);
            } catch (const std::bad_alloc &) {
                throw RestoreError("unable to allocate memory for CMem");
            } catch (const IO::IOError &) {
                throw RestoreError("unexpected eof reading compressed memory");
            }
        }

        if (!uncompress_memory(buf.data(), size)) {
            throw RestoreError("memory cannot be uncompressed");
        }
    } else if (iff.find(IFF::TypeID("UMem"), size)) {
        if (size != header.static_start) {
            throw RestoreError("memory size mismatch");
        }
        try {
            iff.io()->read_exact(memory, header.static_start);
        } catch (const IO::OpenError &) {
            throw RestoreError("unexpected eof reading memory");
        }
    } else {
        throw RestoreError("no memory chunk found");
    }
}

static void read_stks(IFF &iff)
{
    uint32_t size, n = 0, frameno = 0;

    if (!iff.find(IFF::TypeID("Stks"), size)) {
        throw RestoreError("no stacks chunk found");
    }
    if (size == 0) {
        throw RestoreError("empty stacks chunk");
    }

    sp = BASE_OF_STACK;
    fp = BASE_OF_FRAMES;

    while (n < size) {
        uint8_t frame[8];
        uint8_t nlocals;
        uint16_t nstack;
        uint8_t nargs = 0;
        uint32_t frame_pc;

        try {
            iff.io()->read_exact(frame, sizeof frame);
        } catch (const IO::IOError &) {
            throw RestoreError("unexpected eof reading stack frame");
        }
        n += sizeof frame;

        nlocals = frame[3] & 0xf;
        nstack = (frame[6] << 8) | frame[7];
        frame[5]++;
        while ((frame[5] >>= 1) != 0) {
            nargs++;
        }

        frame_pc = (static_cast<uint32_t>(frame[0]) << 16) | (static_cast<uint32_t>(frame[1]) << 8) | static_cast<uint32_t>(frame[2]);
        if (frame_pc >= memory_size) {
            throw RestoreError(fstring("frame #%lu pc out of range (0x%lx)", static_cast<unsigned long>(frameno), static_cast<unsigned long>(frame_pc)));
        }

        add_frame(frame_pc, sp, nlocals, nargs, ((frame[3] & 0x10) == 0x10) ? 0xff + 1 : frame[4]);

        for (int i = 0; i < nlocals; i++) {
            uint16_t l;

            try {
                l = iff.io()->read16();
            } catch (const IO::IOError &) {
                throw RestoreError("unexpected eof reading local variable");
            }
            CURRENT_FRAME->locals[i] = l;

            n += sizeof l;
        }

        for (uint16_t i = 0; i < nstack; i++) {
            uint16_t s;

            try {
                s = iff.io()->read16();
            } catch (const IO::IOError &) {
                throw RestoreError("unexpected eof reading stack entry");
            }
            push_stack(s);

            n += sizeof s;
        }

        frameno++;
    }

    if (n != size) {
        throw RestoreError("stack size mismatch");
    }
}

static void read_args(IFF &iff, SaveOpcode &saveopcode)
{
    uint32_t size;
    uint8_t saveopcode_temp;

    if (!iff.find(IFF::TypeID("Args"), size)) {
        throw RestoreError("no meta save Args chunk found");
    }

    try {
        saveopcode_temp = iff.io()->read8();
    } catch (const IO::IOError &) {
        throw RestoreError("short read in Args");
    }

    saveopcode = static_cast<SaveOpcode>(saveopcode_temp);

    size--;

    // @read takes between 1 and 4 operands, @read_char takes
    // between 1 and 3.
    switch (saveopcode) {
    case SaveOpcode::Read:
        if (size != 2 && size != 4 && size != 6 && size != 8) {
            throw RestoreError(fstring("invalid Args size: %lu", static_cast<unsigned long>(size)));
        }
        break;
    case SaveOpcode::ReadChar:
        if (size != 2 && size != 4 && size != 6) {
            throw RestoreError(fstring("invalid Args size: %lu", static_cast<unsigned long>(size)));
        }
        break;
    default:
        throw RestoreError(fstring("invalid save opcode: %d\n", static_cast<int>(saveopcode)));
    }

    znargs = size / 2;

    for (int i = 0; i < znargs; i++) {
        try {
            zargs[i] = iff.io()->read16();
        } catch (const IO::IOError &) {
            throw RestoreError("short read in Args");
        }
    }
}

static void read_bfzs_specific(IFF &iff, SaveType savetype, SaveOpcode &saveopcode)
{
    uint32_t size;

    read_args(iff, saveopcode);

    if (savetype == SaveType::Autosave && iff.find(IFF::TypeID("Rand"), size)) {
        random_read_rand(*iff.io());
    }

    if (iff.find(IFF::TypeID("Scrn"), size)) {
        // Restoring cannot fail after this, because this function
        // actively touches the screen (changing window
        // configuration, etc). It would be possible to stash the
        // screen state, but it would (potentially) be lossy: if the
        // upper window is reduced, for example, there will be
        // missing text once it is enlarged again.
        try {
            screen_read_scrn(*iff.io(), size);
        } catch (const RestoreError &e) {
            throw RestoreError(fstring("unable to parse screen state: %s", e.what()));
        }
    } else {
        warning("no Scrn chunk in meta save");
    }
}

// Any errors reading the Undo and MSav chunks are ignored. It’s better
// to have a valid autorestore with no undo/user saves than no
// autorestore at all.
static void read_undo_msav(IO &savefile, uint32_t size, SaveStackType type)
{
    bool updated_seen_save_undo = seen_save_undo;

    try {
        uint32_t version = savefile.read32();
        uint32_t count;
        SaveStack &save_stack = save_stacks[type];
        size_t actual_size = 0;
        if (version != 0) {
            return;
        }
        count = savefile.read32();

        actual_size += 4 + 4;

        SaveStack temp = SaveStack();
        temp.max = save_stack.max;
        save_stack.clear();

        for (uint32_t i = 0; i < count; i++) {
            uint8_t savetype = static_cast<uint8_t>(SaveType::Meta);
            std::string desc;
            uint32_t quetzal_size;
            std::vector<uint8_t> quetzal;

            if (type == SaveStackType::Game) {
                savetype = savefile.read8();
                if (static_cast<SaveType>(savetype) != SaveType::Normal && static_cast<SaveType>(savetype) != SaveType::Meta) {

                    return;
                }

                if (static_cast<SaveType>(savetype) == SaveType::Normal) {
                    updated_seen_save_undo = true;
                }

                actual_size += 1;
            } else if (type == SaveStackType::User) {
                desc.resize(savefile.read32());
                savefile.read_exact(&desc[0], desc.size());

                actual_size += 4 + desc.size();
            }

            quetzal_size = savefile.read32();
            quetzal.resize(quetzal_size);
            savefile.read_exact(quetzal.data(), quetzal_size);

            if (count - i <= save_stack.max) {
                SaveState newstate(static_cast<SaveType>(savetype), desc.c_str(), quetzal);
                temp.push(std::move(newstate));
            }

            actual_size += 4 + quetzal_size;
        }

        if (actual_size != size) {
            return;
        }

        save_stack = std::move(temp);
    } catch (const IO::IOError &) {
        return;
    } catch (const std::bad_alloc &) {
        return;
    }

    seen_save_undo = updated_seen_save_undo;
}

static void read_undo(IO &savefile, uint32_t size)
{
    read_undo_msav(savefile, size, SaveStackType::Game);
}

static void read_msav(IO &savefile, uint32_t size)
{
    read_undo_msav(savefile, size, SaveStackType::User);
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

static bool restore_quetzal(const std::shared_ptr<IO> &savefile, SaveType savetype, SaveOpcode &saveopcode, bool close_window)
{
    std::unique_ptr<IFF> iff;
    uint32_t size;
    uint8_t ifhd[13];
    uint32_t newpc;
    bool is_bfzs = savetype == SaveType::Meta || savetype == SaveType::Autosave;
    bool is_bfms = false;
    Stash stash;
    uint16_t flags2 = word(0x10);

    saveopcode = SaveOpcode::None;

    if (is_bfzs) {
        try {
            iff = std::make_unique<IFF>(savefile, IFF::TypeID("BFZS"));
        } catch (const IFF::InvalidFile &) {
            try {
                iff = std::make_unique<IFF>(savefile, IFF::TypeID("BFMS"));
                is_bfms = true;
            } catch (const IFF::InvalidFile &) {
            }
        }
    } else {
        try {
            iff = std::make_unique<IFF>(savefile, IFF::TypeID("IFZS"));
        } catch (const IFF::InvalidFile &) {
        }
    }

    try {
        if (iff == nullptr ||
            !iff->find(IFF::TypeID("IFhd"), size) ||
            size != 13) {

            throw RestoreError("corrupted save file or not a save file at all");
        }

        try {
            iff->io()->read_exact(ifhd, sizeof ifhd);
        } catch (const IO::IOError &) {
            throw RestoreError("corrupted save file or not a save file at all");
        }

        if (((ifhd[0] << 8) | ifhd[1]) != header.release ||
            std::memcmp(&ifhd[2], header.serial, sizeof header.serial) != 0 ||
            ((ifhd[8] << 8) | ifhd[9]) != header.checksum) {

            throw RestoreError("wrong game or version");
        }

        newpc = (static_cast<uint32_t>(ifhd[10]) << 16) | (static_cast<uint32_t>(ifhd[11]) << 8) | static_cast<uint32_t>(ifhd[12]);
        if (newpc >= memory_size) {
            throw RestoreError(fstring("pc out of range (0x%lx)", static_cast<unsigned long>(newpc)));
        }

        if (is_bfms && instruction_has_stack_argument(newpc + 1)) {
            throw RestoreError("detected incompatible meta save: please file a bug report at https://bocfel.org/issues");
        }

        if (is_bfzs && savetype == SaveType::Autosave) {
            if (iff->find(IFF::TypeID("Undo"), size)) {
                read_undo(*iff->io(), size);
            }

            if (iff->find(IFF::TypeID("MSav"), size)) {
                read_msav(*iff->io(), size);
            }
        }

        stash.backup();

        read_mem(*iff);
        read_stks(*iff);

        if (iff->find(IFF::TypeID("Bfnt"), size)) {
            meta_read_bfnt(*iff->io(), size);
        }

        if (iff->find(IFF::TypeID("Bfhs"), size)) {
            if (savetype == SaveType::Autosave || !options.disable_history_playback) {
                try {
                    long start = iff->io()->tell();

                    screen_read_bfhs(*iff->io(), savetype == SaveType::Autosave);

                    if (iff->io()->tell() - start != size) {
                        throw RestoreError("history size mismatch");
                    }
                } catch (const IO::IOError &) {
                    throw RestoreError("can't find location in file");
                }
            }
        } else if (savetype == SaveType::Autosave) {
            warning("unable to find history record");
            screen_print(">");
        }

        if (iff->find(IFF::TypeID("Bfts"), size)) {
            screen_read_bfts(*iff->io(), size);
        }

        if (is_bfzs && !is_bfms) {
            // This must be the last restore call, since it can ultimately
            // wind up modifying the screen state in an irreversible way.
            read_bfzs_specific(*iff, savetype, saveopcode);
        }
    } catch (const RestoreError &e) {
        show_message("Save file error: %s", e.what());

        // If a stash exists, then something vital has been scribbled upon.
        // Try to restore the stash, but if it can’t be restored, the only
        // course of action is to exit.
        if (stash.exists() && !stash.restore()) {
            die("the system is likely in an inconsistent state");
        }

        // If an autosave fails, clear the save stacks: autosaves can
        // contain save data that might have been loaded already. These
        // could use the stash mechanism, but since autosaves are loaded
        // before anything happens, it is guaranteed that the stacks were
        // empty before the restore process started. This is faster and
        // simpler than stashing.
        if (is_bfzs && savetype == SaveType::Autosave) {
            save_stacks[SaveStackType::Game].clear();
            save_stacks[SaveStackType::User].clear();
        }

        return false;
    }

    pc = newpc;

    // §8.6.1.3
    if (close_window && zversion == 3) {
        close_upper_window();
    }

    // For user saves, the game won’t know a restore occurred, so it
    // won’t redraw the status line. Set the “redraw status” bit of
    // Flags 2 to indicate a redraw is requested. The Z-Machine
    // Standards Document says this bit is for V6 only, but Infocom’s
    // documentation says V4+, and A Mind Forever Voyaging (which is V4)
    // checks it.
    if (zversion >= 4 && (savetype == SaveType::Autosave || savetype == SaveType::Meta)) {
        flags2 |= FLAGS2_STATUS;
    }

    // §6.1.2.2: The save might be from a different interpreter with
    // different capabilities, so update the header to indicate what the
    // current capabilities are.
    //
    // Even for in-game saves (either via the game or user stack),
    // rewrite the header: the interpreter will probably be the same,
    // but the screen size might have changed. And, in fact, since these
    // stacks can be stored in autosaves, it’s technically possible they
    // COULD be running in another interpreter, or at least a future
    // version of this one.
    write_header();

    // §6.1.2: Flags 2 should be preserved.
    store_word(0x10, flags2);

    return true;
}

static std::shared_ptr<IO> open_savefile(SaveType savetype, IO::Mode mode)
{
    std::unique_ptr<std::string> filename;

    if (savetype == SaveType::Autosave) {
        filename = zterp_os_autosave_name();
        if (filename == nullptr) {
            return nullptr;
        }
    }

    try {
        return std::make_shared<IO>(filename.get(), mode, IO::Purpose::Save);
    } catch (const IO::OpenError &) {
        if (savetype != SaveType::Autosave) {
            warning("unable to open save file");
        }

        return nullptr;
    }
}

// Perform all aspects of a save, apart from storing/branching.
// Returns true if the save was success, false if not.
bool do_save(SaveType savetype, SaveOpcode saveopcode)
{
    auto savefile = open_savefile(savetype, IO::Mode::WriteOnly);
    if (savefile == nullptr) {
        return false;
    }

    if (!save_quetzal(*savefile, savetype, saveopcode, false)) {
        warning("error while writing save file");
        return false;
    }

    return true;
}

// The suggested filename is ignored, because Glk and, at least as of
// right now, IO, do not provide a method to do this.
// The “prompt” argument added by standard 1.1 is thus also ignored.
void zsave()
{
    if (in_interrupt()) {
        store(0);
        return;
    }

    bool success = do_save(SaveType::Normal, SaveOpcode::None);

    if (zversion <= 3) {
        branch_if(success);
    } else {
        store(success ? 1 : 0);
    }
}

// Perform all aspects of a restore, apart from storing/branching.
// Returns true if the restore was success, false if not.
// saveopcode will be set appropriately based on the type of save file
// this is.
bool do_restore(SaveType savetype, SaveOpcode &saveopcode)
{
    auto savefile = open_savefile(savetype, IO::Mode::ReadOnly);
    if (savefile == nullptr) {
        return false;
    }

    return restore_quetzal(savefile, savetype, saveopcode, true);
}

void zrestore()
{
    SaveOpcode saveopcode;
    bool success = do_restore(SaveType::Normal, saveopcode);

    if (zversion <= 3) {
        branch_if(success);
    } else {
        store(success ? 2 : 0);
    }

    // On a successful restore, we are outside of any interrupt (since
    // @save cannot be called inside an interrupt), so reset the level
    // back to zero.
    if (success) {
        throw Operation::Restore(saveopcode);
    }
}

// Push the current state onto the specified save stack.
SaveResult push_save(SaveStackType type, SaveType savetype, SaveOpcode saveopcode, const char *desc)
{
    SaveStack &s = save_stacks[type];

    if (s.max == 0) {
        return SaveResult::Unavailable;
    }

    try {
        IO savefile(std::vector<uint8_t>(), IO::Mode::WriteOnly);

        if (!save_quetzal(savefile, savetype, saveopcode, true)) {
            return SaveResult::Failure;
        }

        SaveState newstate(savetype, desc, savefile.get_memory());
        s.push(std::move(newstate));

        return SaveResult::Success;
    } catch (const IO::OpenError &) {
    } catch (const std::bad_alloc &) {
    }

    return SaveResult::Failure;
}

// Pop the specified state from the specified stack and jump to it.
// Indices start at 0, with 0 being the most recent save.
bool pop_save(SaveStackType type, size_t saveno, SaveOpcode &saveopcode)
{
    SaveStack &s = save_stacks[type];
    std::shared_ptr<IO> savefile;

    if (saveno >= s.states.size()) {
        return false;
    }

    for (size_t i = 0; i < saveno; i++) {
        s.states.pop_front();
    }

    auto p = std::move(s.states.front());
    s.states.pop_front();

    try {
        savefile = std::make_shared<IO>(p.quetzal, IO::Mode::ReadOnly);
    } catch (const IO::OpenError &) {
        return false;
    }

    return restore_quetzal(savefile, p.savetype, saveopcode, false);
}

// Wrapper around trim_saves which reports failure if the specified save
// does not exist.
bool drop_save(SaveStackType type, size_t i)
{
    SaveStack &s = save_stacks[type];

    if (i > s.states.size()) {
        return false;
    }

    s.trim_saves(i);

    return true;
}

void list_saves(SaveStackType type)
{
    SaveStack &s = save_stacks[type];
    auto nsaves = s.states.size();

    if (nsaves == 0) {
        screen_puts("[No saves available]");
        return;
    }

    for (auto p = s.states.crbegin(); p != s.states.crend(); ++p) {
        std::string desc = std::to_string(nsaves) + ". " + (p->desc.empty() ? "<no description>" : p->desc);

        if (nsaves == 1) {
            desc += " *";
        }

        screen_puts(desc);

        nsaves--;
    }
}

void zsave_undo()
{
    if (in_interrupt()) {
        store(0);
        return;
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
            save_stacks[SaveStackType::Game].clear();
            seen_save_undo = true;
        }

        switch (push_save(SaveStackType::Game, SaveType::Normal, SaveOpcode::None, nullptr)) {
        case SaveResult::Success:
            store(1);
            break;
        case SaveResult::Failure:
            store(0);
            break;
        // @save_undo must return -1 if undo is not available. The only
        // time that’s the case with Bocfel is if the user has requested
        // 0 save slots. Otherwise, @save_undo might still fail as a
        // result of low memory, but that’s a transient failure for
        // which 0 will be returned.
        case SaveResult::Unavailable:
            store(0xffff);
            break;
        }
    }
}

void zrestore_undo()
{
    // If @save_undo has not been called, @restore_undo should fail, even
    // if there are interpreter-generated save states available.
    if (!seen_save_undo) {
        store(0);
    } else {
        SaveOpcode saveopcode;
        bool success = pop_save(SaveStackType::Game, 0, saveopcode);

        store(success ? 2 : 0);

        if (success) {
            throw Operation::Restore(saveopcode);
        }
    }
}

class ArgsStasher : public Stasher {
public:
    void backup() override {
        m_zargs.assign(zargs.begin(), zargs.begin() + znargs);
    }

    bool restore() override {
        std::copy(m_zargs.begin(), m_zargs.end(), zargs.begin());
        znargs = m_zargs.size();

        return true;
    }

    void free() override {
        m_zargs.clear();
    }

private:
    std::vector<uint16_t> m_zargs;
};

class MemoryStasher : public Stasher {
public:
    void backup() override {
        try {
            m_memory = std::make_unique<std::vector<uint8_t>>(memory, memory + header.static_start);
        } catch (std::bad_alloc &) {
            m_memory.reset();
        }
    }

    bool restore() override {
        if (m_memory == nullptr) {
            return false;
        }

        std::copy(m_memory->begin(), m_memory->end(), memory);

        return true;
    }

    void free() override {
        m_memory.reset();
    }

private:
    std::unique_ptr<std::vector<uint8_t>> m_memory;
};

class StackStasher : public Stasher {
public:
    void backup() override {
        try {
            m_stack = std::make_unique<std::vector<uint16_t>>(stack, sp);
        } catch (const std::bad_alloc &) {
            m_stack.reset();
        }
    }

    bool restore() override {
        if (m_stack == nullptr) {
            return false;
        }

        sp = BASE_OF_STACK + m_stack->size();
        std::copy(m_stack->begin(), m_stack->end(), stack);

        return true;
    }

    void free() override {
        m_stack.reset();
    }

private:
    std::unique_ptr<std::vector<uint16_t>> m_stack;
};

class FrameStasher : public Stasher {
public:
    void backup() override {
        try {
            m_frames = std::make_unique<std::vector<CallFrame>>(frames, fp);
        } catch (const std::bad_alloc &) {
            m_frames.reset();
        }
    }

    bool restore() override {
        if (m_frames == nullptr) {
            return false;
        }

        fp = BASE_OF_FRAMES + m_frames->size();
        std::copy(m_frames->begin(), m_frames->end(), frames);

        return true;
    }

    void free() override {
        m_frames.reset();
    }

private:
    std::unique_ptr<std::vector<CallFrame>> m_frames;
};

// Replace with std::clamp when switching to C++17.
template <typename T>
const T &clamp(const T &value, const T &min, const T &max) {
    return value < min ? min : value > max ? max : value;
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
        options.eval_stack_size = clamp<size_t>(options.eval_stack_size, 1, SIZE_MAX / sizeof *stack);
        try {
            stack = new uint16_t[options.eval_stack_size];
        } catch (std::bad_alloc &) {
            die("unable to allocate %lu bytes for the evaluation stack", options.eval_stack_size * static_cast<unsigned long>(sizeof *stack));
        }
        TOP_OF_STACK = &stack[options.eval_stack_size];

        options.call_stack_size = clamp<size_t>(options.call_stack_size, 1, std::min<size_t>(0xffff, (SIZE_MAX / sizeof *frames) - 1));
        try {
            // One extra to help with saving (thus the subtraction of 1
            // above).
            frames = new CallFrame[options.call_stack_size + 1];
        } catch (std::bad_alloc &) {
            die("unable to allocate %lu bytes for the call stack", (options.call_stack_size + 1) * static_cast<unsigned long>(sizeof *frames));
        }
        TOP_OF_FRAMES = &frames[options.call_stack_size];

        stash_register(std::make_unique<ArgsStasher>());
        stash_register(std::make_unique<MemoryStasher>());
        stash_register(std::make_unique<StackStasher>());
        stash_register(std::make_unique<FrameStasher>());
    }

    sp = BASE_OF_STACK;
    fp = BASE_OF_FRAMES;

    // Quetzal requires a dummy frame in non-V6 games, so do that here.
    if (zversion != 6) {
        add_frame(0, sp, 0, 0, 0);
    }

    // Free all @save_undo save states.
    save_stacks[SaveStackType::Game].clear();
    save_stacks[SaveStackType::Game].max = options.undo_slots;
    seen_save_undo = false;

    // Free all /ps save states.
    save_stacks[SaveStackType::User].clear();
    save_stacks[SaveStackType::User].max = 25;
}
