@echo off
rem  Test building a sample game.  We'll make a copy of the sample
rem  game source and build the game.
rem
rem  Usage: testsample <testName>
rem

rem ------ check for options: /Adv3 for an Adv3 sample test
if %1 == /Adv3 goto setup_Adv3

rem ------ general sample - set source to %tstsamp%
set tstsampsrc=%tstsamp%
goto setup_Done

rem ------ /Adv3 option - set source to Adv3 samples
:setup_Adv3
shift
set tstsampsrc=%tstlib%\adv3\samples

:setup_Done

echo Sample Game test: %1

rem  Delete the log file if it already exists
if exist %tstout%\%1.log del %tstout%\%1.log

rem  Copy the sample game to the output directory, renaming to testName.t
copy %tstsampsrc%\%1.t %tstout%\%1.t >nul
if errorlevel 1 echo Error in COPY %tstsampsrc%\%1.t %tstout%\%1.t >%tstout%\%1.log

rem  Compile testName.t with the adv3 library
%tstbin%\capture >>%tstout%\%1.log %tstbin%\t3make -test -a -nobanner -DLANGUAGE=en_us -DMESSAGESTYLE=neu -nopre -Fo %tstout% -Fy %tstout% -o %tstout%\%1.t3 -lib system.tl -lib adv3/adv3.tl -source %tstout%\%1.t

rem  Run the test
%tstbin%\capture >>%tstout%\%1.log %tstbin%\t3run -plain -norand -csl us-ascii -i %tstdat%\%1.in -l%tstout%\%1_run.log %tstout%\%1.t3 -norandomize

rem  Diff the outputs
call %tstbat%\testdiff %1
call %tstbat%\testdiff %1_run

echo.
