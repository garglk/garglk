@echo off
@rem  Pack/Unpack tests.  These are tests do a basic testmake test first,
@rem  then additionally compare a binary output file created by the test
@rem  program against a reference copy.

call %tstbat%\testmake -cp latin1 -pre %1 %1
call %tstbat%\testdiff -bin %1
