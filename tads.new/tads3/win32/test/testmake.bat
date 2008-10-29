@echo off
@rem  Make and execute tests


if %1# == -nodef# goto nodef
if %1# == -debug# goto debug
if %1# == -pre# goto yespre

%tstbin%\capture >%tstout%\%1.log %tstbin%\t3make -test -I%tstdat% -I%tstinc% -a -nobanner -nopre -Fs %tstdat% -Fs %tstlib% -Fo %tstout% -Fy %tstout% -o %tstout%\%1.t3 %2 %3 %4 %5 %6 %7 %8 %9
goto doRun

:nodef

shift
%tstbin%\capture >%tstout%\%1.log %tstbin%\t3make -test -I%tstdat% -I%tstinc% -a -nodef -nobanner -nopre -Fs %tstdat% -Fs %tstlib% -Fo %tstout% -Fy %tstout% -o %tstout%\%1.t3 %2 %3 %4 %5 %6 %7 %8 %9
goto doRun

:debug

shift
%tstbin%\capture >%tstout%\%1.log %tstbin%\t3make -test -d -I%tstdat% -I%tstinc% -a -nobanner -nopre -Fs %tstdat% -Fs %tstlib% -Fo %tstout% -Fy %tstout% -o %tstout%\%1.t3 %2 %3 %4 %5 %6 %7 %8 %9
goto doRun

:yespre

shift
%tstbin%\capture >%tstout%\%1.log %tstbin%\t3make -test -I%tstdat% -I%tstinc% -a -nobanner -Fs %tstdat% -Fs %tstlib% -Fo %tstout% -Fy %tstout% -o %tstout%\%1.t3 %2 %3 %4 %5 %6 %7 %8 %9
goto doRun


:doRun
echo 'Make' test: %1
%tstbin%\capture >>%tstout%\%1.log %tstbin%\test_exec -cs cp437 %tstout%\%1.t3
call %tstbat%\testdiff %1

echo.
