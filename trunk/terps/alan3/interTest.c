/*======================================================================*\

  interTest.c

  Unit tests for inter module in the Alan interpreter

\*======================================================================*/

#include "inter.c"


static void testBlockInstructions()
{
  Aword blockInstructionCode[] = {4, /* Dummy to not execute at zero */
				  4,
				  INSTRUCTION(I_FRAME),
				  4,
				  INSTRUCTION(I_RETURN)};
  Aword localsInstructionCode[] = {4, /* Dummy to not execute at zero */
				   33, /* Value */
				   1, /* Local variable (starts at 1) */
				   0, /* Number of blocks down */
				   INSTRUCTION(I_SETLOCAL),
				   1, /* Local variable (starts at 1) */
				   0, /* Number of blocks down */
				   INSTRUCTION(I_GETLOCAL),
				   INSTRUCTION(I_RETURN)};
  Aint originalSp;

  memory = blockInstructionCode;
  memTop = 100;
  framePointer = 47;
  originalSp = stackp;

  /* Add a block with four local variables */
  interpret(1);

  ASSERT(stackp == originalSp + 1/*old bp*/ + 4/*Locals*/ + 1/*The extra "4"*/);

  memory = localsInstructionCode;
  interpret(1);
  ASSERT(pop() == 33);
}  


static void testLoopInstruction()
{
  Aword loopInstructionCode1[] = {4, /* Dummy to not execute at zero */
				  43, /* Marker */
				  12, /* Limit */
				  1, /* Index */
				  INSTRUCTION(I_LOOP),
				  INSTRUCTION(I_RETURN)};
  memory = loopInstructionCode1;
  interpret(1);			/* Should not do anything */
  ASSERT(pop() == 1 && pop() == 12); /* Index and limit untouched */
  ASSERT(pop() == 43);		/* So the stack should contain the marker */
}


static void testLoopEndInstruction()
{
  Aword loopEndInstructionCode[] = {4, /* Dummy to not execute at zero */
				  1,
				  INSTRUCTION(I_FRAME),
				  4, /* Marker on the stack */
				  12, /* End value */
				  9, /* Start value */
				  INSTRUCTION(I_LOOP),
				  INSTRUCTION(I_DUP),
				  1,
				  0,
				  INSTRUCTION(I_SETLOCAL),
				  INSTRUCTION(I_LOOPEND),
				  INSTRUCTION(I_RETURN)};
  memory = loopEndInstructionCode;
  interpret(1);
  ASSERT(getLocal(0, 1) == 12);
  ASSERT(pop() == 4);
}


static void testGoToLoop() {
  Aword testGoToLoopCode[] = {0,
			      INSTRUCTION(I_LOOP), /* 1 */
			      4,
			      INSTRUCTION(I_LOOP),
			      4,
			      INSTRUCTION(I_LOOPNEXT),
			      INSTRUCTION(I_LOOPNEXT),
			      INSTRUCTION(I_LOOPEND),
			      5,
			      INSTRUCTION(I_LOOPEND)}; /* 9 */
  memory = testGoToLoopCode;
  pc = 9;
  goToLOOP();
  ASSERT(pc == 1);
}


static void testLoopNext() {
  Aword testLoopNextCode[] = {0,
			      INSTRUCTION(I_LOOP),
			      4, /* 2 */
			      INSTRUCTION(I_LOOP),
			      4,
			      INSTRUCTION(I_LOOPNEXT),
			      INSTRUCTION(I_LOOPNEXT),
			      INSTRUCTION(I_LOOPEND),
			      5,
			      INSTRUCTION(I_LOOPEND)}; /* 9 */
  memory = testLoopNextCode;
  pc = 2;
  nextLoop();
  ASSERT(pc == 9);
}

static void testCountInstruction(void)
{
  Aword testCountInstructionCode[] = {0,
				      INSTRUCTION(I_COUNT), /* 7 */
				      INSTRUCTION(I_RETURN)}; /* 8 */
  memory = testCountInstructionCode;

  /* Execute an I_COUNT */
  push(2);			/* Faked COUNT value */
  push(44);			/* Faked limit */
  push(5);			/* Faked loop index */
  interpret(1);
  ASSERT(pop() == 5 && pop() == 44);	/* Values intact */
  ASSERT(pop() == 3);		/* Incremented COUNT */
}

static void testMaxInstruction() {
  Aword testMaxInstructionCode[] = {0,
				    INSTRUCTION(I_MAX),
				    INSTRUCTION(I_RETURN)};
  resetStack();
  push(3);			/* Previous aggregate value */
  push(11);			/* Limit */
  push(12);			/* Index */
  push(2);			/* Attribute value */
  memory = testMaxInstructionCode;
  interpret(1);
  ASSERT(pop() == 12 && pop() == 11);
  ASSERT(pop() == 3);

  push(3);			/* Previous aggregate value */
  push(11);			/* Limit */
  push(12);			/* Index */
  push(4);			/* Attribute value */
  memory = testMaxInstructionCode;
  interpret(1);
  ASSERT(pop() == 12 && pop() == 11);
  ASSERT(pop() == 4);
}

static void testMaxInstance() {
  Aword testMaxInstanceCode[] = {0,
				 CURVAR(V_MAX_INSTANCE),
				 INSTRUCTION(I_RETURN)};
  header->instanceMax = 12;
  memory = testMaxInstanceCode;
  interpret(1);
  ASSERT(pop() == header->instanceMax);
}


void registerInterUnitTests(void)
{
  registerUnitTest(testBlockInstructions);
  registerUnitTest(testGoToLoop);
  registerUnitTest(testLoopNext);
  registerUnitTest(testLoopInstruction);
  registerUnitTest(testLoopEndInstruction);
  registerUnitTest(testMaxInstruction);
  registerUnitTest(testCountInstruction);
  registerUnitTest(testMaxInstance);
}
