rem  Run a Ditch3 (Return to Ditch Day) test script
rem
rem  Usage:  testditch <.cmd-file> <.log-file>
rem
rem  The .cmd 

echo Ditch3 test: %1

rem  Note the base path for the tests
set tstroot=%cd%

rem  Switch to the Ditch3 directory
pushd %tstditch%

rem  Run the script
call  testditch %1.cmd %tstout%\%2.log

rem  Switch back to the test directory
popd

rem  Diff the results
call %tstbat%\testdiff %2
