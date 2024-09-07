@echo off
@rem  Resource bundler test

echo 'Resource' test: %1
set _1=%1

rem  shift all build arguments into a single environment string; stop
rem  at the "-res" flag, which indicates the start of resource arguments

set _src=
:srcLoop
set _src=%_src% %1
shift
if %1# == # goto done
if not %1 == -res goto srcLoop

rem  skip the "-res" flag
shift

rem  run the build
%tstbin%\capture >%tstout%\%_1%.log %tstbin%\t3make -test -I%tstdat% -I%tstinc% -a -nobanner -nopre -Fs %tstdat% -Fs %tstlib% -Fo %tstout% -Fy %tstout% -o %tstout%\%_1%.t3 %_src%

rem  shift all resource bundler arguments into a single environment string

set _res=
:resLoop
set _res=%_res% %1
shift
if not %1# == # goto resLoop

rem  run the resource bundler
%tstbin%\capture >>%tstout%\%_1%.log %tstbin%\t3res %tstout%\%_1%.t3 %_res%

rem  run the program
%tstbin%\capture >>%tstout%\%_1%.log %tstbin%\test_exec -cs cp437 -norand %tstout%\%_1%.t3

rem  check for differences
call %tstbat%\testdiff %_1%

:done
echo.
