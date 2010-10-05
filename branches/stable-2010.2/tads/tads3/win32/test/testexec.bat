@echo off
@rem  Execution tests

echo Execution test: %1

%tstbin%\capture >%tstout%\%1.log %tstbin%\test_prs_top -I%tstinc% %tstdat%\%1.t %tstout%\%1.t3
%tstbin%\capture >>%tstout%\%1.log %tstbin%\test_exec -cs cp437 %tstout%\%1.t3
call %tstbat%\testdiff %1

echo.
