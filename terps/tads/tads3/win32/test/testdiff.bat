@echo off
@rem  Diff a test result against the reference log
@rem
@rem  The reference log is in the %tstlog% directory
@rem  The result file to compare is in the %tstout% directory
@rem
@rem  We'll store a .DIF or a .SUC file in the %tstout% directory
@rem  based on the result
@rem
@rem  -bin option: compare files with names %1.bin, write to %1-bin.suc/.dif

rem presume the diff output file will be %1.suc or %1.dif
set testDiffOut=%1
set testDiffLog=%1.log

rem check for -bin option
if not %1# == -bin# goto noBinOpt
shift
set testDiffOut=%1.bin
set testDiffLog=%1.bin

:noBinOpt

rem clean up any old success/difference files we have hanging around
if exist %tstout%\%testDiffOut%.suc del %tstout%\%testDiffOut%.suc
if exist %tstout%\%testDiffOut%.dif del %tstout%\%testDiffOut%.dif

if exist %tstout%\%testDiffLog% goto do_diff

echo Output file %tstout%\%testDiffLog% not created - test failed > %tstout%\%testDiffOut%.dif
goto done

:do_diff
rem diff the new file against the reference log
gdiff %tstout%\%testDiffLog% %tstlog%\%testDiffLog% > %tstout%\%testDiffOut%.dif
if errorlevel 1 goto hasdiffs

rem success
echo Success > %tstout%\%testDiffOut%.suc
del %tstout%\%testDiffLog%
del %tstout%\%testDiffOut%.dif
goto done

:hasdiffs
rem failure - leave the .LOG and .DIF files in place

:done

