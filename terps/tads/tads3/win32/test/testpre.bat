@echo off
@rem  Preinit tests

echo Preinit test: %1

%tstbin%\capture >%tstout%\%1.log %tstbin%\test_prs_top -I%tstinc% %tstdat%\%1.t %tstout%\%1.t3
%tstbin%\capture >>%tstout%\%1.log %tstbin%\t3pre %tstout%\%1.t3 %tstout%\%1_pre.t3
%tstbin%\capture >>%tstout%\%1.log %tstbin%\test_exec -cs cp437 -norand %tstout%\%1_pre.t3
call %tstbat%\testdiff %1

echo.
