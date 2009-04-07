#!/bin/sh

TEST_DIR=`dirname $0`

echo "Running Alabaster.glorb..."
${TEST_DIR}/../git ${TEST_DIR}/Alabaster.gblorb \
    < ${TEST_DIR}/Alabaster.walk > ${TEST_DIR}/Alabaster.tmp

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
