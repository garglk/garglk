@echo off
rem  Test building a sample game.  We'll make a copy of the sample
rem  game source and build the game.
rem
rem  Usage: testsample <testName>
rem

echo Sample Game test: %1

rem  Delete the log file if it already exists
if exist %tstout%\%1.log del %tstout%\%1.log

rem  Copy the sample game to the output directory, renaming to testName.t
copy %tstsamp%\%1.t %tstout%\%1.t >nul
if errorlevel 1 echo Error in COPY %tstsamp%\%1.t %tstout%\%1.t >%tstout%\%1.log

rem  Compile testName.t with the adv3 library
%tstbin%\capture >>%tstout%\%1.log %tstbin%\t3make -test -a -nobanner -DLANGUAGE=en_us -DMESSAGESTYLE=neu -nopre -Fo %tstout% -Fy %tstout% -o %tstout%\%1.t3 -lib system.tl -lib adv3/adv3.tl -source %tstout%\%1.t

rem  Run the test
%tstbin%\capture >>%tstout%\%1.log %tstbin%\t3run -plain -csl us-ascii -i %tstdat%\%1.in -l%tstout%\%1_run.log %tstout%\%1.t3

rem  Diff the outputs
call %tstbat%\testdiff %1
call %tstbat%\testdiff %1_run

echo.
