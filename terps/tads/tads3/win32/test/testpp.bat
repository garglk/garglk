@echo off
@rem  Preprocessor tests

echo Preprocessor test: %1

%tstbin%\capture >%tstout%\%1.log %tstbin%\test_tok -I%tstinc% -I%tstdat% -P %tstdat%\%1.c
call %tstbat%\testdiff %1

echo.
