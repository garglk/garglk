/* exec.c: Glulxe code for program execution. The main interpreter loop.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://eblong.com/zarf/glulx/index.html
*/

#include "glk.h"
#include "glulxe.h"
#include "opcodes.h"

/* execute_loop():
   The main interpreter loop. This repeats until the program is done.
*/
void execute_loop()
{
  int done_executing = FALSE;
  int ix;
  glui32 opcode;
  operandlist_t *oplist;
  instruction_t inst;
  glui32 value, addr, val0, val1;
  glsi32 vals0, vals1;
  glui32 *arglist;
  glui32 arglistfix[3];

  while (!done_executing) {

    /* Do OS-specific processing, if appropriate. */
    glk_tick();

    /* Fetch the opcode number. */
    opcode = Mem1(pc);
    pc++;
    if (opcode & 0x80) {
      /* More than one-byte opcode. */
      if (opcode & 0x40) {
        /* Four-byte opcode */
        opcode &= 0x3F;
        opcode = (opcode << 8) | Mem1(pc);
        pc++;
        opcode = (opcode << 8) | Mem1(pc);
        pc++;
        opcode = (opcode << 8) | Mem1(pc);
        pc++;
      }
      else {
        /* Two-byte opcode */
        opcode &= 0x7F;
        opcode = (opcode << 8) | Mem1(pc);
        pc++;
      }
    }

    /* Now we have an opcode number. */
    
    /* Fetch the structure that describes how the operands for this
       opcode are arranged. This is a pointer to an immutable, 
       static object. */
    if (opcode < 0x80)
      oplist = fast_operandlist[opcode];
    else
      oplist = lookup_operandlist(opcode);

    if (!oplist)
      fatal_error_i("Encountered unknown opcode.", opcode);

    /* Based on the oplist structure, load the actual operand values
       into inst. This moves the PC up to the end of the instruction. */
    parse_operands(&inst, oplist);

    /* Perform the opcode. This switch statement is split in two, based
       on some paranoid suspicions about the ability of compilers to
       optimize large-range switches. Ignore that. */

    if (opcode < 0x80) {

      switch (opcode) {

      case op_nop:
        break;

      case op_add:
        value = inst.value[0] + inst.value[1];
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_sub:
        value = inst.value[0] - inst.value[1];
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_mul:
        value = inst.value[0] * inst.value[1];
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_div:
        vals0 = inst.value[0];
        vals1 = inst.value[1];
        if (vals1 == 0)
          fatal_error("Division by zero.");
        /* Since C doesn't guarantee the results of division of negative
           numbers, we carefully convert everything to positive values
           first. */
        if (vals1 < 0) {
          vals0 = (-vals0);
          vals1 = (-vals1);
        }
        if (vals0 >= 0) {
          value = vals0 / vals1;
        }
        else {
          value = -((-vals0) / vals1);
        }
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_mod:
        vals0 = inst.value[0];
        vals1 = inst.value[1];
        if (vals1 == 0)
          fatal_error("Division by zero doing remainder.");
        if (vals1 < 0) {
          vals1 = (-vals1);
        }
        if (vals0 >= 0) {
          value = vals0 % vals1;
        }
        else {
          value = -((-vals0) % vals1);
        }
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_neg:
        vals0 = inst.value[0];
        value = (-vals0);
        store_operand(inst.desttype, inst.value[1], value);
        break;

      case op_bitand:
        value = (inst.value[0] & inst.value[1]);
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_bitor:
        value = (inst.value[0] | inst.value[1]);
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_bitxor:
        value = (inst.value[0] ^ inst.value[1]);
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_bitnot:
        value = ~(inst.value[0]);
        store_operand(inst.desttype, inst.value[1], value);
        break;

      case op_shiftl:
        vals0 = inst.value[1];
        if (vals0 < 0 || vals0 >= 32)
          value = 0;
        else
          value = ((glui32)(inst.value[0]) << (glui32)vals0);
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_ushiftr:
        vals0 = inst.value[1];
        if (vals0 < 0 || vals0 >= 32)
          value = 0;
        else
          value = ((glui32)(inst.value[0]) >> (glui32)vals0);
        store_operand(inst.desttype, inst.value[2], value);
        break;
      case op_sshiftr:
        vals0 = inst.value[1];
        if (vals0 < 0 || vals0 >= 32) {
          if (inst.value[0] & 0x80000000)
            value = 0xFFFFFFFF;
          else
            value = 0;
        }
        else {
          /* This is somewhat foolhardy -- C doesn't guarantee that
             right-shifting a signed value replicates the sign bit.
             We'll assume it for now. */
          value = ((glsi32)(inst.value[0]) >> (glsi32)vals0);
        }
        store_operand(inst.desttype, inst.value[2], value);
        break;

      case op_jump:
        value = inst.value[0];
        /* fall through to PerformJump label. */

      PerformJump: /* goto label for successful jumping... ironic, no? */
        if (value == 0 || value == 1) {
          /* Return from function. This is exactly what happens in
             return_op, but it's only a few lines of code, so I won't
             bother with a "goto". */
          leave_function();
          if (stackptr == 0) {
            done_executing = TRUE;
            break;
          }
          pop_callstub(value); /* zero or one */
        }
        else {
          /* Branch to a new PC value. */
          pc = (pc + value - 2);
        }
        break;

      case op_jz:
        if (inst.value[0] == 0) {
          value = inst.value[1];
          goto PerformJump;
        }
        break;
      case op_jnz:
        if (inst.value[0] != 0) {
          value = inst.value[1];
          goto PerformJump;
        }
        break;
      case op_jeq:
        if (inst.value[0] == inst.value[1]) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;
      case op_jne:
        if (inst.value[0] != inst.value[1]) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;
      case op_jlt:
        vals0 = inst.value[0];
        vals1 = inst.value[1];
        if (vals0 < vals1) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;
      case op_jgt:
        vals0 = inst.value[0];
        vals1 = inst.value[1];
        if (vals0 > vals1) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;
      case op_jle:
        vals0 = inst.value[0];
        vals1 = inst.value[1];
        if (vals0 <= vals1) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;
      case op_jge:
        vals0 = inst.value[0];
        vals1 = inst.value[1];
        if (vals0 >= vals1) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;
      case op_jltu:
        val0 = inst.value[0];
        val1 = inst.value[1];
        if (val0 < val1) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;
      case op_jgtu:
        val0 = inst.value[0];
        val1 = inst.value[1];
        if (val0 > val1) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;
      case op_jleu:
        val0 = inst.value[0];
        val1 = inst.value[1];
        if (val0 <= val1) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;
      case op_jgeu:
        val0 = inst.value[0];
        val1 = inst.value[1];
        if (val0 >= val1) {
          value = inst.value[2];
          goto PerformJump;
        }
        break;

      case op_call:
        value = inst.value[1];
        arglist = pop_arguments(value, 0);
        push_callstub(inst.desttype, inst.value[2]);
        enter_function(inst.value[0], value, arglist);
        break;
      case op_return:
        leave_function();
        if (stackptr == 0) {
          done_executing = TRUE;
          break;
        }
        pop_callstub(inst.value[0]);
        break;
      case op_tailcall:
        value = inst.value[1];
        arglist = pop_arguments(value, 0);
        leave_function();
        enter_function(inst.value[0], value, arglist);
        break;

      case op_catch:
        push_callstub(inst.desttype, inst.value[0]);
        value = inst.value[1];
        val0 = stackptr;
        store_operand(inst.desttype, inst.value[0], val0);
        goto PerformJump;
        break;
      case op_throw:
        value = inst.value[0];
        stackptr = inst.value[1];
        pop_callstub(value);
        break;

      case op_copy:
        value = inst.value[0];
        store_operand(inst.desttype, inst.value[1], value);
        break;
      case op_copys:
        value = inst.value[0];
        store_operand_s(inst.desttype, inst.value[1], value);
        break;
      case op_copyb:
        value = inst.value[0];
        store_operand_b(inst.desttype, inst.value[1], value);
        break;

      case op_sexs:
        val0 = inst.value[0];
        if (val0 & 0x8000)
          val0 |= 0xFFFF0000;
        else
          val0 &= 0x0000FFFF;
        store_operand(inst.desttype, inst.value[1], val0);
        break;
      case op_sexb:
        val0 = inst.value[0];
        if (val0 & 0x80)
          val0 |= 0xFFFFFF00;
        else
          val0 &= 0x000000FF;
        store_operand(inst.desttype, inst.value[1], val0);
        break;

      case op_aload:
        value = inst.value[0];
        value += 4 * inst.value[1];
        val0 = Mem4(value);
        store_operand(inst.desttype, inst.value[2], val0);
        break;
      case op_aloads:
        value = inst.value[0];
        value += 2 * inst.value[1];
        val0 = Mem2(value);
        store_operand(inst.desttype, inst.value[2], val0);
        break;
      case op_aloadb:
        value = inst.value[0];
        value += inst.value[1];
        val0 = Mem1(value);
        store_operand(inst.desttype, inst.value[2], val0);
        break;
      case op_aloadbit:
        value = inst.value[0];
        vals0 = inst.value[1];
        val1 = (vals0 & 7);
        if (vals0 >= 0)
          value += (vals0 >> 3);
        else
          value -= ((-1 - vals0) >> 3);
        if (Mem1(value) & (1 << val1))
          val0 = 1;
        else
          val0 = 0;
        store_operand(inst.desttype, inst.value[2], val0);
        break;

      case op_astore:
        value = inst.value[0];
        value += 4 * inst.value[1];
        val0 = inst.value[2];
        MemW4(value, val0);
        break;
      case op_astores:
        value = inst.value[0];
        value += 2 * inst.value[1];
        val0 = inst.value[2];
        MemW2(value, val0);
        break;
      case op_astoreb:
        value = inst.value[0];
        value += inst.value[1];
        val0 = inst.value[2];
        MemW1(value, val0);
        break;
      case op_astorebit:
        value = inst.value[0];
        vals0 = inst.value[1];
        val1 = (vals0 & 7);
        if (vals0 >= 0)
          value += (vals0 >> 3);
        else
          value -= ((-1 - vals0) >> 3);
        val0 = Mem1(value);
        if (inst.value[2])
          val0 |= (1 << val1);
        else
          val0 &= ~((glui32)(1 << val1));
        MemW1(value, val0);
        break;

      case op_stkcount:
        value = (stackptr - valstackbase) / 4;
        store_operand(inst.desttype, inst.value[0], value);
        break;
      case op_stkpeek:
        vals0 = inst.value[0] * 4;
        if (vals0 < 0 || vals0 >= (stackptr - valstackbase))
          fatal_error("Stkpeek outside current stack range.");
        value = Stk4(stackptr - (vals0+4));
        store_operand(inst.desttype, inst.value[1], value);
        break;
      case op_stkswap:
        if (stackptr < valstackbase+8) {
          fatal_error("Stack underflow in stkswap.");
        }
        val0 = Stk4(stackptr-4);
        val1 = Stk4(stackptr-8);
        StkW4(stackptr-4, val1);
        StkW4(stackptr-8, val0);
        break;
      case op_stkcopy:
        vals0 = inst.value[0];
        if (vals0 < 0)
          fatal_error("Negative operand in stkcopy.");
        if (vals0 == 0)
          break;
        if (stackptr < valstackbase+vals0*4)
          fatal_error("Stack underflow in stkcopy.");
        if (stackptr + vals0*4 > stacksize) 
          fatal_error("Stack overflow in stkcopy.");
        addr = stackptr - vals0*4;
        for (ix=0; ix<vals0; ix++) {
          value = Stk4(addr + ix*4);
          StkW4(stackptr + ix*4, value);
        }
        stackptr += vals0*4;
        break;
      case op_stkroll:
        vals0 = inst.value[0];
        vals1 = inst.value[1];
        if (vals0 < 0)
          fatal_error("Negative operand in stkroll.");
        if (stackptr < valstackbase+vals0*4)
          fatal_error("Stack underflow in stkroll.");
        if (vals0 == 0)
          break;
        /* The following is a bit ugly. We want to do vals1 = vals0-vals1,
           because rolling down is sort of easier than rolling up. But
           we also want to take the result mod vals0. The % operator is
           annoying for negative numbers, so we need to do this in two 
           cases. */
        if (vals1 > 0) {
          vals1 = vals1 % vals0;
          vals1 = (vals0) - vals1;
        }
        else {
          vals1 = (-vals1) % vals0;
        }
        if (vals1 == 0)
          break;
        addr = stackptr - vals0*4;
        for (ix=0; ix<vals1; ix++) {
          value = Stk4(addr + ix*4);
          StkW4(stackptr + ix*4, value);
        }
        for (ix=0; ix<vals0; ix++) {
          value = Stk4(addr + (vals1+ix)*4);
          StkW4(addr + ix*4, value);
        }
        break;

      case op_streamchar:
        value = inst.value[0] & 0xFF;
        (*stream_char_handler)(value);
        break;
      case op_streamunichar:
        value = inst.value[0];
        (*stream_unichar_handler)(value);
        break;
      case op_streamnum:
        vals0 = inst.value[0];
        stream_num(vals0, FALSE, 0);
        break;
      case op_streamstr:
        stream_string(inst.value[0], 0, 0);
        break;

      default:
        fatal_error_i("Executed unknown opcode.", opcode);
      }
    }
    else {

      switch (opcode) {

      case op_gestalt:
        value = do_gestalt(inst.value[0], inst.value[1]);
        store_operand(inst.desttype, inst.value[2], value);
        break;

      case op_debugtrap:
        fatal_error_i("user debugtrap encountered.", inst.value[0]);

      case op_jumpabs:
        pc = inst.value[0];
        break;

      case op_callf:
        push_callstub(inst.desttype, inst.value[1]);
        enter_function(inst.value[0], 0, arglistfix);
        break;
      case op_callfi:
        arglistfix[0] = inst.value[1];
        push_callstub(inst.desttype, inst.value[2]);
        enter_function(inst.value[0], 1, arglistfix);
        break;
      case op_callfii:
        arglistfix[0] = inst.value[1];
        arglistfix[1] = inst.value[2];
        push_callstub(inst.desttype, inst.value[3]);
        enter_function(inst.value[0], 2, arglistfix);
        break;
      case op_callfiii:
        arglistfix[0] = inst.value[1];
        arglistfix[1] = inst.value[2];
        arglistfix[2] = inst.value[3];
        push_callstub(inst.desttype, inst.value[4]);
        enter_function(inst.value[0], 3, arglistfix);
        break;

      case op_getmemsize:
        store_operand(inst.desttype, inst.value[0], endmem);
        break;
      case op_setmemsize:
        value = change_memsize(inst.value[0], FALSE);
        store_operand(inst.desttype, inst.value[1], value);
        break;

      case op_getstringtbl:
        value = stream_get_table();
        store_operand(inst.desttype, inst.value[0], value);
        break;
      case op_setstringtbl:
        stream_set_table(inst.value[0]);
        break;

      case op_getiosys:
        stream_get_iosys(&val0, &val1);
        store_operand(inst.desttype, inst.value[0], val0);
        store_operand(inst.desttype, inst.value[1], val1);
        break;
      case op_setiosys:
        stream_set_iosys(inst.value[0], inst.value[1]);
        break;

      case op_glk:
        value = inst.value[1];
        arglist = pop_arguments(value, 0);
        val0 = perform_glk(inst.value[0], value, arglist);
        store_operand(inst.desttype, inst.value[2], val0);
        break;

      case op_random:
        vals0 = inst.value[0];
        if (vals0 == 0)
          value = glulx_random();
        else if (vals0 >= 1)
          value = glulx_random() % (glui32)(vals0);
        else 
          value = -(glulx_random() % (glui32)(-vals0));
        store_operand(inst.desttype, inst.value[1], value);
        break;
      case op_setrandom:
        glulx_setrandom(inst.value[0]);
        break;

      case op_verify:
        value = perform_verify();
        store_operand(inst.desttype, inst.value[0], value);
        break;

      case op_restart:
        vm_restart();
        break;

      case op_protect:
        val0 = inst.value[0];
        val1 = val0 + inst.value[1];
        if (val0 == val1) {
          val0 = 0;
          val1 = 0;
        }
        protectstart = val0;
        protectend = val1;
        break;

      case op_save:
        push_callstub(inst.desttype, inst.value[1]);
        value = perform_save(find_stream_by_id(inst.value[0]));
        pop_callstub(value);
        break;

      case op_restore:
        value = perform_restore(find_stream_by_id(inst.value[0]));
        if (value == 0) {
          /* We've succeeded, and the stack now contains the callstub
             saved during saveundo. Ignore this opcode's operand. */
          value = -1;
          pop_callstub(value);
        }
        else {
          /* We've failed, so we must store the failure in this opcode's
             operand. */
          store_operand(inst.desttype, inst.value[1], value);
        }
        break;

      case op_saveundo:
        push_callstub(inst.desttype, inst.value[0]);
        value = perform_saveundo();
        pop_callstub(value);
        break;

      case op_restoreundo:
        value = perform_restoreundo();
        if (value == 0) {
          /* We've succeeded, and the stack now contains the callstub
             saved during saveundo. Ignore this opcode's operand. */
          value = -1;
          pop_callstub(value);
        }
        else {
          /* We've failed, so we must store the failure in this opcode's
             operand. */
          store_operand(inst.desttype, inst.value[0], value);
        }
        break;

      case op_quit:
        done_executing = TRUE;
        break;

      case op_linearsearch:
        value = linear_search(inst.value[0], inst.value[1], inst.value[2], 
          inst.value[3], inst.value[4], inst.value[5], inst.value[6]);
        store_operand(inst.desttype, inst.value[7], value);
        break;
      case op_binarysearch:
        value = binary_search(inst.value[0], inst.value[1], inst.value[2], 
          inst.value[3], inst.value[4], inst.value[5], inst.value[6]);
        store_operand(inst.desttype, inst.value[7], value);
        break;
      case op_linkedsearch:
        value = linked_search(inst.value[0], inst.value[1], inst.value[2], 
          inst.value[3], inst.value[4], inst.value[5]);
        store_operand(inst.desttype, inst.value[6], value);
        break;

      case op_mzero: {
        glui32 lx;
        glui32 count = inst.value[0];
        addr = inst.value[1];
        for (lx=0; lx<count; lx++, addr++) {
          MemW1(addr, 0);
        }
        }
        break;
      case op_mcopy: {
        glui32 lx;
        glui32 count = inst.value[0];
        glui32 addrsrc = inst.value[1];
        glui32 addrdest = inst.value[2];
        if (addrdest < addrsrc) {
          for (lx=0; lx<count; lx++, addrsrc++, addrdest++) {
            value = Mem1(addrsrc);
            MemW1(addrdest, value);
          }
        }
        else {
          addrsrc += (count-1);
          addrdest += (count-1);
          for (lx=0; lx<count; lx++, addrsrc--, addrdest--) {
            value = Mem1(addrsrc);
            MemW1(addrdest, value);
          }
        }
        }
        break;
      case op_malloc:
        value = heap_alloc(inst.value[0]);
        store_operand(inst.desttype, inst.value[1], value);
        break;
      case op_mfree:
        heap_free(inst.value[0]);
        break;

      default:
        fatal_error_i("Executed unknown opcode.", opcode);
      }
    }
  }
}
