rem  Test restoring a saved state


echo 'Restore' test: %1
%tstbin%\capture >%tstout%\%1.log %tstbin%\test_exec -cs cp437 -norand %tstout%\%2.t3 restore

call %tstbat%\testdiff %1
echo.
