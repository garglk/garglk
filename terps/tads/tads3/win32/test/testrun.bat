@echo off
@rem  Pre-built Run Test: run a test program that's been separately compiled

echo Pre-built Run test: %1

set tstrunlog=%1
shift

%tstbin%\capture >%tstout%\%tstrunlog%.log %tstbin%\test_exec -sd %tstroot% -cs cp437 -norand %tstout%\%1.t3 %2 %3 %4 %5 %6 %7 %8 %9
call %tstbat%\testdiff %tstrunlog%

echo.
