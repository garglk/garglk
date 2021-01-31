/*-
 * Copyright 2010-2016 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2 or 3, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "process.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"

typedef enum {
  StoreWhere_Variable,
  StoreWhere_None,
  StoreWhere_Push,
} StoreWhere;

struct call_frame
{
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

static void PUSH_STACK(uint16_t n) { ZASSERT(sp != TOP_OF_STACK, "stack overflow"); *sp++ = n; }
static uint16_t POP_STACK(void) { ZASSERT(sp > CURRENT_FRAME->sp, "stack underflow"); return *--sp; }

struct save_state
{
  bool is_meta;
  uint8_t *quetzal;
  long quetzal_size;

  time_t when;
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

static struct save_state *new_save_state(void)
{
  struct save_state *new;

  new = malloc(sizeof *new);
  if(new != NULL)
  {
    new->quetzal = NULL;
    new->desc = NULL;
    new->when = time(NULL);
  }

  return new;
}

static void free_save_state(struct save_state *s)
{
  if(s != NULL)
  {
    free(s->quetzal);
    free(s);
  }
}

static void clear_save_stack(struct save_stack *s)
{
  while(s->head != NULL)
  {
    struct save_state *tmp = s->head;
    s->head = s->head->next;
    free_save_state(tmp);
  }
  s->tail = NULL;
  s->count = 0;
}

void init_stack(void)
{
  /* Allocate space for the evaluation and call stacks.
   * Clamp the size between 1 and the largest value that will not
   * produce an overflow of size_t when multiplied by the size of the
   * type.
   * Also, the call stack can be no larger than 0xffff so that the
   * result of a @catch will fit into a 16-bit integer.
   */
  if(stack == NULL)
  {
#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define CLAMP(n, a, b)	((n) < (a) ? (a) : (n) > (b) ? (b): (n))
    options.eval_stack_size = CLAMP(options.eval_stack_size, 1, SIZE_MAX / sizeof *stack);
    stack = malloc(options.eval_stack_size * sizeof *stack);
    if(stack == NULL) die("unable to allocate %lu bytes for the evaluation stack", options.eval_stack_size * (unsigned long)sizeof *stack);
    TOP_OF_STACK = &stack[options.eval_stack_size];

    options.call_stack_size = CLAMP(options.call_stack_size, 1, MIN(0xffff, (SIZE_MAX / sizeof *frames) - sizeof *frames));
    /* One extra to help with saving (thus the subtraction of sizeof *frames above). */
    frames = malloc((options.call_stack_size + 1) * sizeof *frames);
    if(frames == NULL) die("unable to allocate %lu bytes for the call stack", (options.call_stack_size + 1) * (unsigned long)sizeof *frames);
    TOP_OF_FRAMES = &frames[options.call_stack_size];
#undef MIN
#undef CLAMP
  }

  sp = BASE_OF_STACK;
  fp = BASE_OF_FRAMES;

  /* Quetzal requires a dummy frame in non-V6 games, so do that here. */
  if(zversion != 6) add_frame(0, sp, 0, 0, 0);

  /* Free all @save_undo save states. */
  clear_save_stack(&save_stacks[SAVE_GAME]);
  save_stacks[SAVE_GAME].max = options.max_saves;
  seen_save_undo = false;

  /* Free all /ps save states. */
  clear_save_stack(&save_stacks[SAVE_USER]);
  save_stacks[SAVE_USER].max = 25;
}

uint16_t variable(uint16_t var)
{
  ZASSERT(var < 0x100, "unable to decode variable %u", (unsigned)var);

  /* Stack */
  if(var == 0)
  {
    return POP_STACK();
  }

  /* Locals */
  else if(var <= 0x0f)
  {
    ZASSERT(var <= CURRENT_FRAME->nlocals, "attempting to read from nonexistent local variable %d: routine has %d", (int)var, CURRENT_FRAME->nlocals);
    return CURRENT_FRAME->locals[var - 1];
  }

  /* Globals */
  else if(var <= 0xff)
  {
    var -= 0x10;
    return word(header.globals + (var * 2));
  }

  /* This is an “impossible” situation (ie, the game did something wrong).
   * It will be caught above if safety checks are turned on, but if they
   * are not, do what we can: lie.
   */
  return -1;
}

void store_variable(uint16_t var, uint16_t n)
{
  ZASSERT(var < 0x100, "unable to decode variable %u", (unsigned)var);

  /* Stack */
  if(var == 0)
  {
    PUSH_STACK(n);
  }

  /* Locals */
  else if(var <= 0x0f)
  {
    ZASSERT(var <= CURRENT_FRAME->nlocals, "attempting to store to nonexistent local variable %d: routine has %d", (int)var, CURRENT_FRAME->nlocals);
    CURRENT_FRAME->locals[var - 1] = n;
  }

  /* Globals */
  else if(var <= 0xff)
  {
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
  PUSH_STACK(zargs[0]);
}

void zpull(void)
{
  uint16_t v;

  if(zversion != 6)
  {
    v = POP_STACK();

    /* The z-spec 1.1 requires indirect variable references to the stack not to push/pop */
    if(zargs[0] == 0) *stack_top_element() = v;
    else              store_variable(zargs[0], v);
  }
  else
  {
    if(znargs == 0)
    {
      v = POP_STACK();
    }
    else
    {
      uint16_t slots = user_word(zargs[0]) + 1;

      v = user_word(zargs[0] + (2 * slots));

      user_store_word(zargs[0], slots);
    }

    store(v);
  }
}

void zload(void)
{
  /* The z-spec 1.1 requires indirect variable references to the stack not to push/pop */
  if(zargs[0] == 0) store(*stack_top_element());
  else              store(variable(zargs[0]));
}

void zstore(void)
{
  /* The z-spec 1.1 requires indirect variable references to the stack not to push/pop */
  if(zargs[0] == 0) *stack_top_element() = zargs[1];
  else              store_variable(zargs[0], zargs[1]);
}

static void call(StoreWhere store_where)
{
  uint32_t jmp_to;
  uint8_t nlocals;
  uint16_t where;

  if(zargs[0] == 0)
  {
    /* call(StoreWhere_Push) should never happen if zargs[0] is 0. */
    if(store_where == StoreWhere_Variable) store(0);
    return;
  }

  jmp_to = unpack_routine(zargs[0]);
  ZASSERT(jmp_to < memory_size - 1, "call to invalid address 0x%lx", (unsigned long)jmp_to);

  nlocals = byte(jmp_to++);
  ZASSERT(nlocals <= 15, "too many (%d) locals at 0x%lx", nlocals, (unsigned long)jmp_to - 1);

  if(zversion <= 4) ZASSERT(jmp_to + (nlocals * 2) < memory_size, "call to invalid address 0x%lx", (unsigned long)jmp_to);

  switch(store_where)
  {
    case StoreWhere_Variable: where = byte(pc++); break; /* Where to store return value */
    case StoreWhere_None:     where = 0xff + 1;   break; /* Or a tag meaning no return value */
    case StoreWhere_Push:     where = 0xff + 2;   break; /* Or a tag meaning push the return value */
    default:                  die("internal error: invalid store_where value (%d)", (int)store_where);
  }

  add_frame(pc, sp, nlocals, znargs - 1, where);

  for(int i = 0; i < nlocals; i++)
  {
    if(i < znargs - 1)
    {
      CURRENT_FRAME->locals[i] = zargs[i + 1];
    }
    else
    {
      if(zversion <= 4) CURRENT_FRAME->locals[i] = word(jmp_to + (2 * i));
      else              CURRENT_FRAME->locals[i] = 0;
    }
  }

  /* Take care of locals! */
  if(zversion <= 4) jmp_to += nlocals * 2;

  pc = jmp_to;
}

void start_v6(void)
{
  call(StoreWhere_None);
}

#ifdef ZTERP_GLK
uint16_t direct_call(uint16_t routine)
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

  return POP_STACK();
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

  if(where <= 0xff)
  {
    store_variable(where, retval);
  }
  else if(where == 0xff + 2)
  {
    PUSH_STACK(retval);
    interrupt_return();
  }
}

void zret_popped(void)
{
  do_return(POP_STACK());
}

void zpop(void)
{
  POP_STACK();
}

void zcatch(void)
{
  ZASSERT(zversion == 6 || NFRAMES > 1, "@catch called outside of a function");

  /* Must account for the dummy frame in non-V6 stories. */
  store(zversion == 6 ? NFRAMES : NFRAMES - 1);
}

void zthrow(void)
{
  /* As with @catch, account for the dummy frame. */
  if(zversion != 6) zargs[1]++;

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
  if(znargs == 1)
  {
    for(uint16_t i = 0; i < zargs[0]; i++) POP_STACK();
  }
  else
  {
    user_store_word(zargs[1], user_word(zargs[1]) + zargs[0]);
  }
}

void zpush_stack(void)
{
  uint16_t slots = user_word(zargs[1]);

  if(slots == 0)
  {
    branch_if(false);
    return;
  }

  user_store_word(zargs[1] + (2 * slots), zargs[0]);
  user_store_word(zargs[1], slots - 1);

  branch_if(true);
}

/* Compress dynamic memory according to Quetzal.  Memory is allocated
 * for the passed-in pointer, and must be freed by the caller.  The
 * return value is the size of compressed memory, or 0 on failure.
 */
static uint32_t compress_memory(uint8_t **compressed)
{
  uint32_t ret = 0;
  long i = 0;
  uint8_t *tmp;

  /* The output buffer needs to be 1.5× the size of dynamic memory for
   * the worst-case scenario: every other byte in memory differs from
   * the story file.  This will cause every other byte to take up two
   * bytes in the output, thus creating 3 bytes of output for every 2 of
   * input.  This should round up for the extreme case of alternating
   * zero/non-zero bytes with zeroes at the beginning and end, but due
   * to the fact that trailing zeroes are not stored, it does not need
   * to.
   */
  tmp = malloc((3 * header.static_start) / 2);
  if(tmp == NULL) return 0;

  while(true)
  {
    long run = i;

    /* Count zeroes.  Stop counting when:
     * • The end of dynamic memory is reached, or
     * • A non-zero value is found
     */
    while(i < header.static_start && (byte(i) ^ dynamic_memory[i]) == 0)
    {
      i++;
    }

    run = i - run;

    /* A run of zeroes at the end need not be written. */
    if(i == header.static_start) break;

    /* If there has been a run of zeroes, write them out
     * 256 at a time.
     */
    while(run > 0)
    {
      tmp[ret++] = 0;
      tmp[ret++] = (run > 256 ? 255 : run - 1);
      run -= 256;
    }

    /* The current byte differs from the story, so write it. */
    tmp[ret++] = byte(i) ^ dynamic_memory[i];

    i++;
  }

  /* There is a small chance that ret will be 0 (if everything in
   * dynamic memory is identical to what was initially loaded from the
   * story).  If that's the case, this realloc() will instead behave as
   * free(), so just allocate one more than is necessary to avoid a
   * potential double free.
   */
  *compressed = realloc(tmp, ret + 1);
  if(*compressed == NULL) *compressed = tmp;

  return ret;
}

/* Reverse of the above function. */
static bool uncompress_memory(const uint8_t *compressed, uint32_t size)
{
  uint32_t memory_index = 0;

  memcpy(memory, dynamic_memory, header.static_start);

  for(uint32_t i = 0; i < size; i++)
  {
    if(compressed[i] != 0)
    {
      if(memory_index == header.static_start) return false;
      store_byte(memory_index, byte(memory_index) ^ compressed[i]);
      memory_index++;
    }
    else
    {
      if(++i == size) return false;

      if(memory_index + (compressed[i] + 1) > header.static_start) return false;
      memory_index += (compressed[i] + 1);
    }
  }

  return true;
}

static char (*write_ifhd(zterp_io *savefile))[5]
{
  zterp_io_write16(savefile, header.release);
  zterp_io_write_exact(savefile, header.serial, 6);
  zterp_io_write16(savefile, header.checksum);
  zterp_io_write8(savefile, (pc >> 16) & 0xff);
  zterp_io_write8(savefile, (pc >>  8) & 0xff);
  zterp_io_write8(savefile, (pc >>  0) & 0xff);

  return &"IFhd";
}

/* Store the filename in an IntD chunk. */
static char (*write_intd(zterp_io *savefile))[5]
{
  zterp_io_write_exact(savefile, "UNIX", 4);
  zterp_io_write8(savefile, 0x02);
  zterp_io_write8(savefile, 0);
  zterp_io_write16(savefile, 0);
  zterp_io_write_exact(savefile, "    ", 4);
  zterp_io_write_exact(savefile, game_file, strlen(game_file));

  return &"IntD";
}

static char (*write_mem(zterp_io *savefile))[5]
{
  uint8_t *compressed = NULL;
  uint32_t memsize = compress_memory(&compressed);
  const uint8_t *mem = memory;
  char (*type)[5];

  /* It is possible for the compressed memory size to be larger than
   * uncompressed; in this case, just store the uncompressed memory.
   */
  if(memsize > 0 && memsize < header.static_start)
  {
    mem = compressed;
    type = &"CMem";
  }
  else
  {
    memsize = header.static_start;
    type = &"UMem";
  }
  zterp_io_write_exact(savefile, mem, memsize);
  free(compressed);

  return type;
}

/* Quetzal save/restore functions. */
static char (*write_stks(zterp_io *savefile))[5]
{
  /* Add one more “fake” call frame with just enough information to
   * calculate the evaluation stack used by the current routine.
   */
  fp->sp = sp;
  for(struct call_frame *p = BASE_OF_FRAMES; p != fp; p++)
  {
    uint8_t temp;

    zterp_io_write8(savefile, (p->pc >> 16) & 0xff);
    zterp_io_write8(savefile, (p->pc >>  8) & 0xff);
    zterp_io_write8(savefile, (p->pc >>  0) & 0xff);

    temp = p->nlocals;
    if(p->where > 0xff) temp |= 0x10;
    zterp_io_write8(savefile, temp);

    if(p->where > 0xff) zterp_io_write8(savefile, 0);
    else                zterp_io_write8(savefile, p->where);

    zterp_io_write8(savefile, (1U << p->nargs) - 1);

    /* number of words of evaluation stack used */
    zterp_io_write16(savefile, (p + 1)->sp - p->sp);

    /* local variables */
    for(int i = 0; i < p->nlocals; i++) zterp_io_write16(savefile, p->locals[i]);

    /* evaluation stack */
    for(ptrdiff_t i = 0; i < (p + 1)->sp - p->sp; i++) zterp_io_write16(savefile, p->sp[i]);
  }

  return &"Stks";
}

static char (*write_args(zterp_io *savefile))[5]
{
  for(int i = 0; i < znargs; i++) zterp_io_write16(savefile, zargs[i]);

  return &"Args";
}

static void write_chunk(zterp_io *io, char (*(*writefunc)(zterp_io *))[5])
{
  long chunk_pos, end_pos, size;
  char (*type)[5];

  chunk_pos = zterp_io_tell(io);
  zterp_io_write32(io, 0); /* type */
  zterp_io_write32(io, 0); /* size */
  type = writefunc(io);
  end_pos = zterp_io_tell(io);
  size = end_pos - chunk_pos - 8;
  zterp_io_seek(io, chunk_pos, SEEK_SET);
  zterp_io_write_exact(io, *type, 4);
  zterp_io_write32(io, size);
  zterp_io_seek(io, end_pos, SEEK_SET);
  if(size & 1) zterp_io_write8(io, 0); /* padding */
}

/* Meta saves (generated by the interpreter) are based on Quetzal. The
 * format of the save state is the same (that is, the IFhd, IntD, and
 * CMem/UMem chunks are identical). The type of the save file itself is
 * BFZS instead of IFZS to prevent the files from being used by a normal
 * @restore (as they are not compatible).
 *
 * BFZS has the following changes/extensions to Quetzal:
 *
 * The save file is from the middle of a read instruction instead of at
 * a save. PC points to either the store byte of the @sread
 * instruction, or if @aread, the next instruction. Instead of
 * continuing at @restore, the read needs to be restarted.
 *
 * There are two new chunks associated with BFZS:
 *
 * Args:
 * This represents the arguments to the current @read opcode. The size
 * of the chunk is the number of arguments multiplied by two, followed
 * by the arguments (as 16-bit unsigned integers).
 *
 * Scrn:
 * This is a versioned chunk that represents the screen state.  It
 * consists of the following:
 *
 * • Version (uint32_t)
 * • The currently-selected window (uint8_t)
 * • The height of the upper window (uint16_t)
 * • The x coordinate of the upper window cursor (uint16_t)
 * • The y coordinate of the upper window cursor (uint16_t)
 *
 * Coordinates are stored as Z-machine coordinates, meaning (1, 1) is
 * the origin. If no coordinates are available (e.g. if there is no
 * upper window), they will be (0, 0).
 *
 * This is followed by 2 or 8 identical sections representing the
 * windows. V6 has 8 windows, all others have 2. Windows consist of the
 * following:
 *
 * • Style (uint8_t)
 * • Font (uint8_t)
 * • Previous font (uint8_t)
 * • Foreground color
 * • Background color
 *
 * Colors are three bytes:
 *
 * • Mode (uint8_t): 0 = ANSI, 1 = True
 * • Color (uint16_t): 1-12 for ANSI, 0-32767 for True
 *
 * The Scrn chunk is optional. If it is not present, the current screen
 * state will be maintained. More importantly, unsupported Scrn versions
 * are not fatal. This allows older versions of Bocfel to load newer
 * meta saves that include updated versions of the Scrn chunk.
 */
static bool save_quetzal(zterp_io *savefile, bool is_meta, bool store_history)
{
  long file_size;

  if(!zterp_io_try(savefile)) return false;

  zterp_io_write_exact(savefile, "FORM", 4);
  zterp_io_write32(savefile, 0); /* to be filled in */
  zterp_io_write_exact(savefile, is_meta ? "BFZS" : "IFZS", 4);

  write_chunk(savefile, write_ifhd);
  write_chunk(savefile, write_intd);
  write_chunk(savefile, write_mem);
  write_chunk(savefile, write_stks);

  if(store_history) write_chunk(savefile, screen_write_bfhs);

  /* When restoring a meta save, @read will be called to bring the user
   * back to the same place as the save occurred. While memory and the
   * stack will be restored properly, the arguments to @read will not
   * (as the normal Z-machine save/restore mechanism doesn’t need them:
   * all restoring does is store or branch, using the location of the
   * program counter after restore). If this is a meta save, store zargs
   * so it can be restored before re-entering @read.
   */
  if(is_meta)
  {
    write_chunk(savefile, write_args);
    write_chunk(savefile, screen_write_scrn);
  }

  file_size = zterp_io_tell(savefile);
  zterp_io_seek(savefile, 4, SEEK_SET);
  zterp_io_write32(savefile, file_size - 8); /* entire file size minus 8 (FORM + size) */

  return true;
}

/* Restoring can put the system in an inconsistent state by restoring
 * only part of memory: the save file may be corrupt and cause failure
 * part way through updating memory, for example.  This set of functions
 * takes a snapshot of the current state of dynamic memory and the
 * stacks so they can be restored on failure.
 */
static uint8_t *memory_backup;
static uint16_t *stack_backup;
static int stack_backup_size;
static struct call_frame *frames_backup;
static long frames_backup_size;

static void memory_snapshot_free(void)
{
  free(memory_backup);
  free(stack_backup);
  free(frames_backup);

  memory_backup = NULL;
  stack_backup = NULL;
  frames_backup = NULL;
}

static void memory_snapshot(void)
{
  memory_snapshot_free();

  memory_backup = malloc(header.static_start);
  if(memory_backup == NULL) goto err;

  memcpy(memory_backup, memory, header.static_start);

  stack_backup_size = sp - stack;
  if(stack_backup_size != 0)
  {
    stack_backup = malloc(stack_backup_size * sizeof *stack);
    if(stack_backup == NULL) goto err;
    memcpy(stack_backup, stack, stack_backup_size * sizeof *stack);
  }

  frames_backup_size = NFRAMES;
  if(frames_backup_size != 0)
  {
    frames_backup = malloc(frames_backup_size * sizeof *frames);
    if(frames_backup == NULL) goto err;
    memcpy(frames_backup, frames, frames_backup_size * sizeof *frames);
  }

  return;

err:
  memory_snapshot_free();
}

static bool memory_restore(void)
{
  /* stack_backup and frames_backup will be NULL if the stacks were
   * empty, so use memory_backup to determine if a snapshot has been
   * taken.
   */
  if(memory_backup == NULL) return false;

  memcpy(memory, memory_backup, header.static_start);
  if(stack_backup != NULL) memcpy(stack, stack_backup, stack_backup_size * sizeof *stack);
  sp = stack + stack_backup_size;
  if(frames_backup != NULL) memcpy(frames, frames_backup, frames_backup_size * sizeof *frames);
  fp = frames + frames_backup_size;

  memory_snapshot_free();

  return true;
}

#define goto_err(...)	do { show_message("Save file error: " __VA_ARGS__); goto err; } while(false)
#define goto_death(...)	do { show_message("Save file error: " __VA_ARGS__); goto death; } while(false)

/* Earlier versions of Bocfel’s meta-saves neglected to take into
 * account the fact that the call to @read may have included arguments
 * pulled from the stack. Any such arguments are lost in an old-style
 * save, meaning the save file is not usable. However, most calls to
 * @read do not use the stack, so most old-style saves should work just
 * fine. This function is used to determine whether the @read
 * instruction used for the meta-save pulled from the stack. If so, the
 * restore will fail, but otherwise, it will proceed.
 */
static bool instruction_has_stack_argument(uint32_t addr)
{
  uint32_t types = user_byte(addr++);

  for(int i = 6; i >= 0; i -= 2)
  {
    switch((types >> i) & 0x03)
    {
      case 0:
        addr += 2;
        break;
      case 1:
        addr++;
        break;
      case 2:
        if(user_byte(addr++) == 0) return true;
        break;
      default:
        return false;
    }
  }

  return false;
}

static bool restore_quetzal(zterp_io *savefile, bool is_meta, bool *is_bfms)
{
  zterp_iff *iff;
  uint32_t size;
  uint32_t n = 0;
  uint8_t ifhd[13];
  uint32_t newpc;
  uint16_t saved_args[znargs];
  int saved_nargs = znargs;
  int frameno = 0;

  *is_bfms = false;

  memcpy(saved_args, zargs, sizeof saved_args);

  if(is_meta)
  {
    iff = zterp_iff_parse(savefile, "BFZS");
    if(iff == NULL)
    {
      iff = zterp_iff_parse(savefile, "BFMS");
      if(iff != NULL) *is_bfms = true;
    }
  }
  else
  {
    iff = zterp_iff_parse(savefile, "IFZS");
  }

  if(iff == NULL ||
     !zterp_iff_find(iff, "IFhd", &size) ||
     size != 13 ||
     !zterp_io_read_exact(savefile, ifhd, sizeof ifhd))
  {
    goto_err("corrupted save file or not a save file at all");
  }

  if(((ifhd[0] << 8) | ifhd[1]) != header.release ||
     memcmp(&ifhd[2], header.serial, sizeof header.serial) != 0 ||
     ((ifhd[8] << 8) | ifhd[9]) != header.checksum)
  {
    goto_err("wrong game or version");
  }

  newpc = (ifhd[10] << 16) | (ifhd[11] << 8) | ifhd[12];
  if(newpc >= memory_size) goto_err("pc out of range (0x%lx)", (unsigned long)newpc);

  if(*is_bfms && instruction_has_stack_argument(newpc + 1))
  {
    goto_err("detected incompatible meta save: please file a bug report at https://bocfel.org/issues");
  }

  memory_snapshot();

  if(zterp_iff_find(iff, "CMem", &size))
  {
    uint8_t *buf;

    /* Dynamic memory is 64KB, and a worst-case save should take up
     * 1.5× that value, or 96KB.  Simply double the 64KB to avoid
     * potential edge-case problems.
     */
    if(size > 131072) goto_err("reported CMem size too large (%lu bytes)", (unsigned long)size);

    buf = malloc(size * sizeof *buf);
    if(buf == NULL) goto_err("unable to allocate memory for decompression");

    if(!zterp_io_read_exact(savefile, buf, size))
    {
      free(buf);
      goto_err("unexpected eof reading compressed memory");
    }

    if(!uncompress_memory(buf, size))
    {
      free(buf);
      goto_death("memory cannot be uncompressed");
    }

    free(buf);
  }
  else if(zterp_iff_find(iff, "UMem", &size))
  {
    if(size != header.static_start) goto_err("memory size mismatch");
    if(!zterp_io_read_exact(savefile, memory, header.static_start)) goto_death("unexpected eof reading memory");
  }
  else
  {
    goto_err("no memory chunk found");
  }

  if(!zterp_iff_find(iff, "Stks", &size)) goto_death("no stacks chunk found");
  if(size == 0) goto_death("empty stacks chunk");

  sp = BASE_OF_STACK;
  fp = BASE_OF_FRAMES;

  while(n < size)
  {
    uint8_t frame[8];
    uint8_t nlocals;
    uint16_t nstack;
    uint8_t nargs = 0;
    uint32_t frame_pc;

    if(!zterp_io_read_exact(savefile, frame, sizeof frame)) goto_death("unexpected eof reading stack frame");
    n += sizeof frame;

    nlocals = frame[3] & 0xf;
    nstack = (frame[6] << 8) | frame[7];
    frame[5]++;
    while((frame[5] >>= 1) != 0) nargs++;

    frame_pc = (frame[0] << 16) | (frame[1] << 8) | frame[2];
    if(frame_pc >= memory_size) goto_err("frame #%d pc out of range (0x%lx)", frameno, (unsigned long)frame_pc);

    add_frame(frame_pc, sp, nlocals, nargs, (frame[3] & 0x10) ? 0xff + 1 : frame[4]);

    for(int i = 0; i < nlocals; i++)
    {
      uint16_t l;

      if(!zterp_io_read16(savefile, &l)) goto_death("unexpected eof reading local variable");
      CURRENT_FRAME->locals[i] = l;

      n += sizeof l;
    }

    for(uint16_t i = 0; i < nstack; i++)
    {
      uint16_t s;

      if(!zterp_io_read16(savefile, &s)) goto_death("unexpected eof reading stack entry");
      PUSH_STACK(s);

      n += sizeof s;
    }

    frameno++;
  }

  if(n != size) goto_death("stack size mismatch");

  if(is_meta && !*is_bfms)
  {
    if(!zterp_iff_find(iff, "Args", &size)) goto_death("no meta save Args chunk found");

    /* @read takes between 1 and 4 operands. */
    if (size != 2 && size != 4 && size != 6 && size != 8) goto_death("invalid Args size: %lu", (unsigned long)size);
    znargs = size / 2;

    for(int i = 0; i < znargs; i++)
    {
      if(!zterp_io_read16(savefile, &zargs[i])) goto_death("short read in Args");
    }

    if(zterp_iff_find(iff, "Scrn", &size))
    {
      char err[64];

      if(!screen_read_scrn(savefile, size, err, sizeof err)) goto_death("unable to parse screen state: %s", err);
    }
    else
    {
      warning("no Scrn chunk in meta save");
    }
  }

  if(!options.disable_history_playback && zterp_iff_find(iff, "Bfhs", &size))
  {
    long start = zterp_io_tell(savefile);

    screen_read_bfhs(savefile);

    if(zterp_io_tell(savefile) - start != size) goto_death("history size mismatch");
  }

  zterp_iff_free(iff);
  memory_snapshot_free();

  pc = newpc;

  return true;

death:
  /* At this point, something vital (memory and/or the stacks) has been
   * scribbed upon; if there was a successful backup, restore it.
   * Otherwise the only course of action is to exit.
   */
  if(!memory_restore()) die("the system is likely in an inconsistent state");

err:
  /* A snapshot may have been taken, but neither memory nor the stacks
   * have been overwritten, so just free the snapshot. Restore zargs in
   * case this was a meta save that updated it before failure.
   */
  memory_snapshot_free();
  zterp_iff_free(iff);
  memcpy(zargs, saved_args, sizeof saved_args);
  znargs = saved_nargs;
  return false;
}

#undef goto_err
#undef goto_death

/* Perform all aspects of a save, apart from storing/branching.
 * Returns true if the save was success, false if not.
 * “is_meta” is true if this save file is from a meta-save.
 */
bool do_save(bool is_meta)
{
  zterp_io *savefile;
  bool success;

  savefile = zterp_io_open(NULL, ZTERP_IO_WRONLY, ZTERP_IO_SAVE);
  if(savefile == NULL)
  {
    warning("unable to open save file");
    return false;
  }

  success = save_quetzal(savefile, is_meta, true);

  zterp_io_close(savefile);

  return success;
}

/* The suggested filename is ignored, because Glk and, at least as of
 * right now, zterp_io_open(), do not provide a method to do this.
 * The “prompt” argument added by standard 1.1 is thus also ignored.
 */
void zsave(void)
{
  if(in_interrupt()) die("@save called inside of an interrupt");

  bool success = do_save(false);

  if(zversion <= 3) branch_if(success);
  else              store(success ? 1 : 0);
}

/* Perform all aspects of a restore, apart from storing/branching.
 * Returns true if the restore was success, false if not.
 * “is_meta” is true if this save file is expected to be from a
 * meta-save. *is_bfms will be set to true if this is an old-style BFMS
 * meta-save, otherwise false.
 */
bool do_restore(bool is_meta, bool *is_bfms)
{
  zterp_io *savefile;
  uint16_t flags2;
  bool success;

  savefile = zterp_io_open(NULL, ZTERP_IO_RDONLY, ZTERP_IO_SAVE);
  if(savefile == NULL)
  {
    warning("unable to open save file");
    return false;
  }

  flags2 = word(0x10);

  success = restore_quetzal(savefile, is_meta, is_bfms);

  zterp_io_close(savefile);

  if(success)
  {
    /* If there are any pending read events, cancel on successful restore. */
    cancel_all_events();

    /* §8.6.1.3 */
    if(zversion == 3) close_upper_window();

    /* §6.1.2.2: The save might be from a different interpreter with
     * different capabilities, so update the header to indicate what the
     * current capabilities are...
     */
    write_header();

    /* ...except that flags2 should be preserved (§6.1.2). */
    store_word(0x10, flags2);

    /* Redraw the status line in games that use one. */
    if(zversion <= 3) zshow_status();
  }

  return success;
}

void zrestore(void)
{
  bool success = do_restore(false, &(bool){false});

  if(zversion <= 3) branch_if(success);
  else              store(success ? 2 : 0);

  /* On a successful restore, we are outside of any interrupt (since
   * @save cannot be called inside an interrupt), so reset the level
   * back to zero.
   */
  if(success) interrupt_reset(false);
}

/* Push the current state onto the specified save stack. */
bool push_save(enum save_type type, bool is_meta, const char *desc)
{
  struct save_stack *s = &save_stacks[type];
  struct save_state *new;
  zterp_io *savefile = NULL;

  if(options.max_saves == 0) return false;

  new = new_save_state();
  if(new == NULL) return false;

  if(desc != NULL)
  {
    new->desc = malloc(strlen(desc) + 1);
    if(new->desc != NULL) strcpy(new->desc, desc);
  }
  else
  {
    new->desc = NULL;
  }

  new->is_meta = is_meta;
  savefile = zterp_io_open_memory(NULL, 0);
  if(savefile == NULL) goto err;

  if(!save_quetzal(savefile, new->is_meta, false)) goto err;

  if(!zterp_io_close_memory(savefile, &new->quetzal, &new->quetzal_size)) goto err;

  /* If the maximum number has been reached, drop the last element.
   * A negative value for max_saves means there is no maximum.
   *
   * Small note: calling @restore_undo twice should succeed both times
   * if @save_undo was called twice (or three, four, etc. times).  If
   * there aren’t enough slots, however, @restore_undo calls that should
   * work will fail because the earlier save states will have been
   * dropped.  This can easily be seen by running TerpEtude’s undo test
   * with the slot count set to 1.  By default, the number of save slots
   * is 100, so this will not be an issue unless a game goes out of its
   * way to cause problems.
   */
  if(s->max > 0 && s->count == s->max)
  {
    struct save_state *tmp = s->tail;
    s->tail = s->tail->prev;
    if(s->tail == NULL) s->head = NULL;
    else                s->tail->next = NULL;
    free_save_state(tmp);
    s->count--;
  }

  new->next = s->head;
  new->prev = NULL;
  if(new->next != NULL) new->next->prev = new;
  s->head = new;
  if(s->tail == NULL) s->tail = new;

  s->count++;

  return true;

err:
  if(savefile != NULL) zterp_io_close(savefile);
  free_save_state(new);

  return false;
}

/* Remove the first “n” saves from the specified stack.  If there are
 * not enough saves available, remove all saves.
 */
static void trim_saves(enum save_type type, long n)
{
  struct save_stack *s = &save_stacks[type];

  while(n-- > 0 && s->count > 0)
  {
    struct save_state *tmp = s->head;

    s->head = s->head->next;
    s->count--;
    free_save_state(tmp);
    if(s->head != NULL) s->head->prev = NULL;
    if(s->count == 0) s->tail = NULL;
  }
}

/* Pop the specified state from the specified stack and jump to it.
 * Indices start at 0, with 0 being the most recent save.
 */
bool pop_save(enum save_type type, long saveno, bool *call_zread)
{
  struct save_stack *s = &save_stacks[type];
  struct save_state *p = s->head;
  uint16_t flags2;
  zterp_io *savefile;

  if(saveno >= s->count) return false;

  flags2 = word(0x10);

  for(long i = 0; i < saveno; i++) p = p->next;

  *call_zread = p->is_meta;

  savefile = zterp_io_open_memory(p->quetzal, p->quetzal_size);
  if(savefile == NULL) return false;

  if(!restore_quetzal(savefile, p->is_meta, &(bool){false}))
  {
    zterp_io_close(savefile);
    return false;
  }

  zterp_io_close(savefile);

  trim_saves(type, saveno + 1);

  /* §6.1.2.2: As with @restore, header values marked with Rst (in
   * §11) should be reset.  Unlike with @restore, it can be assumed
   * that the game was saved by the same interpreter, but it cannot be
   * assumed that the screen size is the same as it was at the time
   * @save_undo was called.
   */
  write_header();

  /* §6.1.2: Flags 2 should be preserved. */
  store_word(0x10, flags2);

  return true;
}

/* Wrapper around trim_saves which reports failure if the specified save
 * does not exist.
 */
bool drop_save(enum save_type type, long i)
{
  struct save_stack *s = &save_stacks[type];

  if(i > s->count) return false;

  trim_saves(type, i);

  return true;
}

void list_saves(enum save_type type, void (*printer)(const char *))
{
  struct save_stack *s = &save_stacks[type];
  long nsaves = s->count;

  if(nsaves == 0)
  {
    printer("[No saves available]");
    return;
  }

  for(struct save_state *p = s->tail; p != NULL; p = p->prev)
  {
    char formatted_time[128];
    /* “buf” will store either the formatted time or the user-provided
     * description; the description comes from @read, which will never
     * read more than 256 characters.  Pad to accommodate the index,
     * newline, and potential asterisk.
     */
    char buf[300];
    struct tm *tm;
    const char *desc;

    tm = localtime(&p->when);
    if(tm == NULL || strftime(formatted_time, sizeof formatted_time, "%c", tm) == 0)
    {
      snprintf(formatted_time, sizeof formatted_time, "<no time information>");
    }

    if(p->desc != NULL) desc = p->desc;
    else                desc = formatted_time;

    snprintf(buf, sizeof buf, "%ld. %s%s", nsaves--, desc, p->prev == NULL ? " *" : "");
    printer(buf);
  }
}

void zsave_undo(void)
{
  if(in_interrupt()) die("@save_undo called inside of an interrupt");

  /* If override undo is set, all calls to @save_undo are reported as
   * failure; the interpreter still reports that @save_undo is
   * available, however.  These values will be tuned if it becomes
   * necessary (i.e. if some games behave badly).
   */
  if(options.override_undo)
  {
    store(0);
  }
  else
  {
    /* On the first call to @save_undo, switch over to game-based save
     * states instead of interpreter-generated save states.
     */
    if(!seen_save_undo)
    {
      clear_save_stack(&save_stacks[SAVE_GAME]);
      seen_save_undo = true;
    }

    store(push_save(SAVE_GAME, false, NULL));
  }
}

void zrestore_undo(void)
{
  /* If @save_undo has not been called, @restore_undo should fail, even
   * if there are interpreter-generated save states available.
   */
  if(!seen_save_undo)
  {
    store(0);
  }
  else
  {
    bool call_zread;
    bool success = pop_save(SAVE_GAME, 0, &call_zread);

    store(success ? 2 : 0);

    if(success) interrupt_reset(call_zread);
  }
}
