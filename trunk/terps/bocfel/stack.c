/*-
 * Copyright 2010-2011 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <setjmp.h>

#include "stack.h"
#include "branch.h"
#include "iff.h"
#include "io.h"
#include "memory.h"
#include "process.h"
#include "screen.h"
#include "util.h"
#include "zterp.h"

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

static struct save_state
{
  uint32_t pc;

  uint32_t memsize;
  uint8_t *memory;

  uint32_t stack_size;
  uint16_t *stack;

  long nframes;
  struct call_frame *frames;

  struct save_state *prev, *next;
} *saves_head, *saves_tail;

static long nsaves;

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

  /* Free all previous save states (from @save_undo). */
  while(saves_head != NULL)
  {
    struct save_state *tmp = saves_head;
    saves_head = saves_head->next;
    free(tmp->stack);
    free(tmp->frames);
    free(tmp->memory);
    free(tmp);
  }
  saves_tail = NULL;
  nsaves = 0;
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
    return WORD(header.globals + (var * 2));
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

  /* Stack. */
  if(var == 0)
  {
    PUSH_STACK(n);
  }

  /* Local variables. */
  else if(var <= 0x0f)
  {
    ZASSERT(var <= CURRENT_FRAME->nlocals, "attempting to store to nonexistent local variable %d: routine has %d", (int)var, CURRENT_FRAME->nlocals);
    CURRENT_FRAME->locals[var - 1] = n;
  }

  /* Global variables. */
  else if(var <= 0xff)
  {
    var -= 0x10;
    STORE_WORD(header.globals + (var * 2), n);
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

void call(int do_store)
{
  uint32_t jmp_to;
  uint8_t nlocals;
  uint16_t where;

  if(zargs[0] == 0)
  {
    /* call(2) should never happen if zargs[0] is 0. */
    if(do_store) store(0);
    return;
  }

  jmp_to = unpack(zargs[0], 0);
  ZASSERT(jmp_to < memory_size - 1, "call to invalid address 0x%lx", (unsigned long)jmp_to);

  nlocals = BYTE(jmp_to++);
  ZASSERT(nlocals <= 15, "too many (%d) locals at 0x%lx", nlocals, (unsigned long)jmp_to - 1);

  if(zversion <= 4) ZASSERT(jmp_to + (nlocals * 2) < memory_size, "call to invalid address 0x%lx", (unsigned long)jmp_to);

  switch(do_store)
  {
    case 1:  where = BYTE(pc++); break; /* Where to store return value */
    case 0:  where = 0xff + 1;   break; /* Or a tag meaning no return value */
    default: where = 0xff + 2;   break; /* Or a tag meaning push the return value */
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
      if(zversion <= 4) CURRENT_FRAME->locals[i] = WORD(jmp_to + (2 * i));
      else              CURRENT_FRAME->locals[i] = 0;
    }
  }

  /* Take care of locals! */
  if(zversion <= 4) jmp_to += nlocals * 2;

  pc = jmp_to;
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
  call(2);

  process_instructions();

  memcpy(zargs, saved_args, sizeof saved_args);
  znargs = saved_nargs;

  return POP_STACK();
}
#endif

void zcall_store(void)
{
  call(1);
}
void zcall_nostore(void)
{
  call(0);
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
    break_from(interrupt_level());
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
  ZASSERT(zversion == 6 || NFRAMES > 1, "@check_arg_count called outside of a function");

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
    branch_if(0);
    return;
  }

  user_store_word(zargs[1] + (2 * slots), zargs[0]);
  user_store_word(zargs[1], slots - 1);

  branch_if(1);
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

  while(1)
  {
    long run = i;

    /* Count zeroes.  Stop counting when:
     * • The end of dynamic memory is reached
     * • A non-zero value is found
     */
    while(i < header.static_start && (BYTE(i) ^ dynamic_memory[i]) == 0)
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
    tmp[ret++] = BYTE(i) ^ dynamic_memory[i];

    i++;
  }

  *compressed = realloc(tmp, ret);
  if(*compressed == NULL) *compressed = tmp;

  return ret;
}

/* Reverse of the above function. */
static int uncompress_memory(const uint8_t *compressed, uint32_t size)
{
  uint32_t memory_index = 0;

  memcpy(memory, dynamic_memory, header.static_start);

  for(uint32_t i = 0; i < size; i++)
  {
    if(compressed[i] != 0)
    {
      if(memory_index == header.static_start) return -1;
      STORE_BYTE(memory_index, BYTE(memory_index) ^ compressed[i]);
      memory_index++;
    }
    else
    {
      if(++i == size) return -1;

      if(memory_index + (compressed[i] + 1) > header.static_start) return -1;
      memory_index += (compressed[i] + 1);
    }
  }

  return 0;
}

/* Push the current game state onto the game-state stack. */
static int push_save(void)
{
  struct save_state *new;

  if(options.max_saves == 0) return -1;

  new = malloc(sizeof *new);
  if(new == NULL) goto err;
  new->stack = NULL;
  new->frames = NULL;

  new->pc = pc;

  new->stack_size = sp - BASE_OF_STACK;
  new->stack = malloc(new->stack_size * sizeof *new->stack);
  if(new->stack == NULL) goto err;
  memcpy(new->stack, BASE_OF_STACK, new->stack_size * sizeof *new->stack);

  new->nframes = NFRAMES;
  new->frames = malloc(new->nframes * sizeof *new->frames);
  if(new->frames == NULL) goto err;
  memcpy(new->frames, BASE_OF_FRAMES, new->nframes * sizeof *new->frames);

  if(options.disable_undo_compression)
  {
    new->memory = malloc(header.static_start);
    if(new->memory == NULL) goto err;
    memcpy(new->memory, memory, header.static_start);
  }
  else
  {
    new->memsize = compress_memory(&new->memory);
    if(new->memsize == 0) goto err;
  }

  /* If the maximum number has been reached, drop the last element.
   * A negative value for max_saves means there is no maximum.
   */
  if(options.max_saves > 0 && nsaves == options.max_saves)
  {
    struct save_state *tmp = saves_tail;
    saves_tail = saves_tail->prev;
    if(saves_tail == NULL) saves_head = NULL;
    else                   saves_tail->next = NULL;
    free(tmp->stack);
    free(tmp->frames);
    free(tmp->memory);
    free(tmp);
    nsaves--;
  }

  new->next = saves_head;
  new->prev = NULL;
  if(new->next != NULL) new->next->prev = new;
  saves_head = new;
  if(saves_tail == NULL) saves_tail = new;

  nsaves++;

  return 1;

err:
  if(new != NULL)
  {
    free(new->stack);
    free(new->frames);
    free(new);
  }

  return 0;
}

/* Pop the last-stored game state and jump to it. */
static int pop_save(void)
{
  struct save_state *p;

  if(nsaves == 0) return 0;

  p = saves_head;

  pc = p->pc;

  if(options.disable_undo_compression)
  {
    memcpy(memory, p->memory, header.static_start);
  }
  else
  {
    /* If this fails it’s a bug: unlike Quetzal files, the contents of
     * p->memory are known to be good, because the compression was done
     * by us with no chance for corruption (apart, again, from bugs).
     */
    if(uncompress_memory(p->memory, p->memsize) == -1) die("error uncompressing memory: unable to continue");
  }

  sp = BASE_OF_STACK + p->stack_size;
  memcpy(BASE_OF_STACK, p->stack, sizeof *sp * p->stack_size);

  fp = BASE_OF_FRAMES + p->nframes;
  memcpy(BASE_OF_FRAMES, p->frames, sizeof *p->frames * p->nframes);

  /* Never pop off the last state.  A story has every right to call
   * @restore_undo as many times as it called @save_undo.  However, if
   * there aren’t enough save slots, popping off the last state would
   * cause @restore_undo to return failure when it should not.
   */
  if(nsaves > 1)
  {
    saves_head = saves_head->next;
    saves_head->prev = NULL;

    free(p->stack);
    free(p->frames);
    free(p->memory);
    free(p);

    nsaves--;
  }

  return 2;
}

void zsave_undo(void)
{
  if(interrupt_level() != 0) die("@save_undo called inside of an interrupt");

  store(push_save());
}

void zrestore_undo(void)
{
  uint16_t flags2;

  /* §6.1.2: Flags 2 should be preserved. */
  flags2 = WORD(0x10);
  store(pop_save());
  STORE_WORD(0x10, flags2);
}

/* Quetzal save/restore functions. */
static jmp_buf exception;
#define WRITE8(v)  do { uint8_t  v_ = (v); if(zterp_io_write(savefile, &v_, sizeof v_) != sizeof v_) longjmp(exception, 1); local_written += 1; } while(0)
#define WRITE16(v) do { uint16_t w_ = (v); WRITE8(w_ >>  8); WRITE8(w_ & 0xff); } while(0)
#define WRITE32(v) do { uint32_t x_ = (v); WRITE8(x_ >> 24); WRITE8((x_ >> 16) & 0xff); WRITE8((x_ >> 8) & 0xff); WRITE8(x_ & 0xff); } while(0)
#define WRITEID(v) WRITE32(STRID(v))

static size_t quetzal_write_stack(zterp_io *savefile)
{
  size_t local_written = 0;

  /* Add one more “fake” call frame with just enough information to
   * calculate the evaluation stack used by the current routine.
   */
  fp->sp = sp;
  for(struct call_frame *p = BASE_OF_FRAMES; p != fp; p++)
  {
    uint8_t temp;

    WRITE8((p->pc >> 16) & 0xff);
    WRITE8((p->pc >>  8) & 0xff);
    WRITE8((p->pc >>  0) & 0xff);

    temp = p->nlocals;
    if(p->where > 0xff) temp |= 0x10;
    WRITE8(temp);

    if(p->where > 0xff) WRITE8(0);
    else                WRITE8(p->where);

    WRITE8((1U << p->nargs) - 1);

    /* number of words of evaluation stack used */
    WRITE16((p + 1)->sp - p->sp);

    /* local variables */
    for(int i = 0; i < p->nlocals; i++) WRITE16(p->locals[i]);

    /* evaluation stack */
    for(ptrdiff_t i = 0; i < (p + 1)->sp - p->sp; i++) WRITE16(p->sp[i]);
  }

  return local_written;
}

static int save_quetzal(zterp_io *savefile)
{
  if(setjmp(exception) != 0) return 0;

  size_t local_written = 0;
  size_t game_len;
  uint32_t memsize;
  uint8_t *compressed;
  uint8_t *mem = memory;
  long stks_pos;
  size_t stack_size;

  WRITEID("FORM");
  WRITEID("    "); /* to be filled in */
  WRITEID("IFZS");

  WRITEID("IFhd");
  WRITE32(13);
  WRITE16(header.release);
  zterp_io_write(savefile, header.serial, 6);
  local_written += 6;
  WRITE16(header.checksum);
  WRITE8(pc >> 16);
  WRITE8(pc >> 8);
  WRITE8(pc & 0xff);
  WRITE8(0); /* padding */

  /* Store the filename in an IntD chunk. */
  game_len = 12 + strlen(game_file);
  WRITEID("IntD");
  WRITE32(game_len);
  WRITEID("UNIX");
  WRITE8(0x02);
  WRITE8(0);
  WRITE16(0);
  WRITEID("    ");
  zterp_io_write(savefile, game_file, game_len - 12);
  local_written += (game_len - 12);
  if(game_len & 1) WRITE8(0);

  memsize = compress_memory(&compressed);

  /* It is possible for the compressed memory size to be larger than
   * uncompressed; in this case, just store the uncompressed memory.
   */
  if(memsize > 0 && memsize < header.static_start)
  {
    mem = compressed;
    WRITEID("CMem");
  }
  else
  {
    memsize = header.static_start;
    WRITEID("UMem");
  }
  WRITE32(memsize);
  zterp_io_write(savefile, mem, memsize);
  local_written += memsize;
  if(memsize & 1) WRITE8(0); /* padding */
  free(compressed);

  WRITEID("Stks");
  stks_pos = zterp_io_tell(savefile);
  WRITEID("    "); /* to be filled in */
  stack_size = quetzal_write_stack(savefile);
  local_written += stack_size;
  if(stack_size & 1) WRITE8(0); /* padding */

  zterp_io_seek(savefile, 4, SEEK_SET);
  WRITE32(local_written - 8); /* entire file size minus 8 (FORM + size) */

  zterp_io_seek(savefile, stks_pos, SEEK_SET);
  WRITE32(stack_size); /* size of the stacks chunk */

  return 1;
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
static int frames_backup_size;

static void memory_snapshot_free(void)
{
  free(memory_backup);
  free(stack_backup);
  free(frames_backup);

  memory_backup = NULL;
  stack_backup = NULL;
  frames_backup = NULL;
}

static int memory_snapshot(void)
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

  frames_backup_size = fp - frames;
  if(frames_backup_size != 0)
  {
    frames_backup = malloc(frames_backup_size * sizeof *frames);
    if(frames_backup == NULL) goto err;
    memcpy(frames_backup, frames, frames_backup_size * sizeof *frames);
  }

  return 1;

err:
  memory_snapshot_free();

  return 0;
}

static int memory_restore(void)
{
  /* stack_backup and frames_backup will be NULL if the stacks were
   * empty, so use memory_backup to determine if a snapshot has been
   * taken.
   */
  if(memory_backup == NULL) return 0;

  memcpy(memory, memory_backup, header.static_start);
  if(stack_backup != NULL) memcpy(stack, stack_backup, stack_backup_size * sizeof *stack);
  sp = stack + stack_backup_size;
  if(frames_backup != NULL) memcpy(frames, frames_backup, frames_backup_size * sizeof *frames);
  fp = frames + frames_backup_size;

  memory_snapshot_free();

  return 1;
}

#define goto_err(...)	do { show_message("save file error: " __VA_ARGS__); goto err; } while(0)
#define goto_death(...)	do { show_message("save file error: " __VA_ARGS__); goto death; } while(0)

static int restore_quetzal(zterp_io *savefile)
{
  zterp_iff *iff;
  uint32_t size;
  uint32_t n = 0;
  uint8_t ifhd[13];

  iff = zterp_iff_parse(savefile, "IFZS");

  if(iff == NULL ||
     !zterp_iff_find(iff, "IFhd", &size) ||
     size != 13 ||
     zterp_io_read(savefile, ifhd, sizeof ifhd) != sizeof ifhd)
  {
    goto_err("corrupted save file or not a save file at all");
  }

  if(((ifhd[0] << 8) | ifhd[1]) != header.release ||
     memcmp(&ifhd[2], header.serial, sizeof header.serial) != 0 ||
     ((ifhd[8] << 8) | ifhd[9]) != header.checksum)
  {
    goto_err("wrong game or version");
  }

  memory_snapshot();

  if(zterp_iff_find(iff, "CMem", &size))
  {
    uint8_t buf[size]; /* Too big for the stack? */

    if(zterp_io_read(savefile, buf, size) != size) goto_err("unexpected eof reading compressed memory");

    if(uncompress_memory(buf, size) == -1) goto_death("memory cannot be uncompressed");
  }
  else if(zterp_iff_find(iff, "UMem", &size))
  {
    if(size != header.static_start) goto_err("memory size mismatch");
    if(zterp_io_read(savefile, memory, header.static_start) != header.static_start) goto_death("unexpected eof reading memory");
  }
  else
  {
    goto_err("no memory chunk found");
  }

  if(!zterp_iff_find(iff, "Stks", &size)) goto_death("no stacks chunk found");

  sp = BASE_OF_STACK;
  fp = BASE_OF_FRAMES;

  while(n < size)
  {
    uint8_t frame[8];
    uint8_t nlocals;
    uint16_t nstack;
    uint8_t nargs = 0;

    if(zterp_io_read(savefile, frame, sizeof frame) != sizeof frame) goto_death("unexpected eof reading stack frame");
    n += sizeof frame;

    nlocals = frame[3] & 0xf;
    nstack = (frame[6] << 8) | frame[7];
    frame[5]++;
    while(frame[5] >>= 1) nargs++;

    add_frame((frame[0] << 16) | (frame[1] << 8) | frame[2], sp, nlocals, nargs, (frame[3] & 0x10) ? 0xff + 1 : frame[4]);

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
  }

  if(n != size) goto_death("stack size mismatch");

  zterp_iff_free(iff);
  memory_snapshot_free();

  pc = (ifhd[10] << 16) | (ifhd[11] << 8) | ifhd[12];

  return 1;

death:
  /* At this point, something vital (memory and/or the stacks) has been
   * scribbed upon; if there was a successful backup, restore it.
   * Otherwise the only course of action is to exit.
   */
  if(!memory_restore()) die("the system is likely in an inconsistent state");

err:
  /* A snapshot may have been taken, but neither memory nor the stacks
   * have been overwritten, so just free the snapshot.
   */
  memory_snapshot_free();
  zterp_iff_free(iff);
  return 0;
}

#undef goto_err
#undef goto_death

/* The suggested filename is ignored, because GLK and, at least as of
 * right now, zterp_io_open(), do not provide a method to do this.
 * The “prompt” argument added by standard 1.1 is thus also ignored.
 */
void zsave(void)
{
  zterp_io *savefile;
  int success;

  if(interrupt_level() != 0) die("@save called inside of an interrupt");

  savefile = zterp_io_open(NULL, ZTERP_IO_WRONLY | ZTERP_IO_SAVE);
  if(savefile == NULL)
  {
    warning("unable to open save file");

    if(zversion <= 3) branch_if(0);
    else              store(0);

    return;
  }

  success = save_quetzal(savefile);

  zterp_io_close(savefile);

  if(zversion <= 3) branch_if(success);
  else              store(success);
}

void zrestore(void)
{
  zterp_io *savefile;
  uint16_t flags2;
  int success;

  savefile = zterp_io_open(NULL, ZTERP_IO_RDONLY | ZTERP_IO_SAVE);
  if(savefile == NULL)
  {
    warning("unable to open save file");

    if(zversion <= 3) branch_if(0);
    else              store(0);

    return;
  }

  flags2 = WORD(0x10);

  success = restore_quetzal(savefile);

  zterp_io_close(savefile);

  if(success)
  {
    /* §8.6.1.3 */
    if(zversion == 3) close_upper_window();

    /* The save might be from a different interpreter with different
     * capabilities, so update the header to indicate what the current
     * capabilities are...
     */
    write_header();

    /* ...except that flags2 should be preserved (§6.1.2). */
    STORE_WORD(0x10, flags2);

    /* Redraw the status line in games that use one. */
    if(zversion <= 3) zshow_status();
  }

  if(zversion <= 3) branch_if(success);
  else              store(success ? 2 : 0);
}
