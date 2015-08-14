#!/bin/sh

TEST_DIR=`dirname $0`

echo "Running Alabaster.glorb..."
TIME_1=`date +%s%3N`
${TEST_DIR}/../git ${TEST_DIR}/Alabaster.gblorb \
    < ${TEST_DIR}/Alabaster.walk > ${TEST_DIR}/Alabaster.tmp
TIME_2=`date +%s%3N`
TIME_DIFF=`expr $TIME_2 - $TIME_1`
echo "Running Alabaster.glorb took $TIME_DIFF milliseconds."

echo "Comparing output against Alabaster.golden..."
if diff -q ${TEST_DIR}/Alabaster.tmp ${TEST_DIR}/Alabaster.golden; then
  echo "TEST PASSED"
else
  echo
  echo "*** TEST FAILED ***"
  echo
  echo "The Alabaster walkthrough is not producing the same output as before."
  echo "Please check Alabaster.tmp manually."
fi
