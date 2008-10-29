@echo off
@rem  Diff a test result against the reference log
@rem
@rem  The reference log is in the %tstlog% directory
@rem  The result file to compare is in the %tstout% directory
@rem
@rem  We'll store a .DIF or a .SUC file in the %tstout% directory
@rem  based on the result

rem clean up any old success/difference files we have hanging around
if exist %tstout%\%1.suc del %tstout%\%1.suc
if exist %tstout%\%1.dif del %tstout%\%1.dif

if exist %tstout%\%1.log goto do_diff

echo Output file %tstout%\%1.log not created - test failed > %tstout%\%1.dif
goto done

:do_diff
rem diff the new file against the reference log
gdiff %tstout%\%1.log %tstlog%\%1.log > %tstout%\%1.dif
if errorlevel 1 goto hasdiffs

rem success
echo Success > %tstout%\%1.suc
del %tstout%\%1.log
del %tstout%\%1.dif
goto done

:hasdiffs
rem failure - leave the .LOG and .DIF files in place

:done

