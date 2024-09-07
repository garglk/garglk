@echo off
@rem  Make and execute tests


sleep -m 500

set tstCodePage=cp437
if not %1# == -cp# goto nocp
shift
set tstCodePage=%1
shift

:nocp
set tstmakerun=YES
if not %1# == -norun# goto yesrun
shift
set tstmakerun=NO

:yesrun
set tstscript=
if not %1# == -script# goto noscript
shift
set tstscript=YES

:noscript
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
if %tstmakerun%==NO goto :doSkipRun
echo 'Make' test: %1

if %tstscript%# == YES# set tstscript=-I %tstdat%\%1.in
%tstbin%\capture >>%tstout%\%1.log %tstbin%\test_exec -sd %tstroot% %tstscript% -cs %tstCodePage% -norand %tstout%\%1.t3


:doSkipRun
call %tstbat%\testdiff %1

echo.
