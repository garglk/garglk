/*======================================================================*\

  sysdepTest.c

  Unit tests for sysdep module in the Alan interpreter

\*======================================================================*/

#include "sysdep.c"


static void testStringEqualsIgnoringCase()
{
  ASSERT(compareStrings("abcd", "abcd")==0);
  ASSERT(compareStrings("abcd", "Abcd")==0);
  ASSERT(compareStrings("abcd", "aBcd")==0);
  ASSERT(compareStrings("abcd", "abCd")==0);
  ASSERT(compareStrings("abcd", "abcD")==0);
  ASSERT(compareStrings("abcd", "abcd")==0);
  ASSERT(compareStrings("ABCD", "Abcd")==0);
  ASSERT(compareStrings("ABCD", "aBcd")==0);
  ASSERT(compareStrings("ABCD", "abCd")==0);
  ASSERT(compareStrings("ABCD", "abcD")==0);
  ASSERT(compareStrings("abcd", "abcde")!=0);
  ASSERT(compareStrings("abcde", "Abcd")!=0);
  ASSERT(compareStrings("abc", "aBcd")!=0);
  ASSERT(compareStrings("abd", "abCd")!=0);
  ASSERT(compareStrings("bcd", "abcD")!=0);
}

static void testIsLowerCase() 
{
  ASSERT(isLowerCase(246));
}


void registerSysdepUnitTests()
{
  registerUnitTest(testIsLowerCase);
  registerUnitTest(testStringEqualsIgnoringCase);
}
