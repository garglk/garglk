@echo off
@echo.

call win32\test\test_env

rem Delete old log, object, symbol, and image files
echo Cleaning up old test output files...
del %tstout%\*.dif
del %tstout%\*.suc
del %tstout%\*.log
del %tstout%\*.t3s
del %tstout%\*.t3o
del %tstout%\*.t3
echo.

rem object test - doesn't produce any output; mostly check for no crashiness
echo Object test
%tstbin%\test_obj
echo.

rem Preprocessor tests

call %tstbat%\testpp ansi
call %tstbat%\testpp circ
call %tstbat%\testpp circ2
call %tstbat%\testpp embed
call %tstbat%\testpp define
call %tstbat%\testpp ifdef
call %tstbat%\testpp concat
call %tstbat%\testpp varmacpp

rem Execution tests

call %tstbat%\testexec basic
call %tstbat%\testexec finally
call %tstbat%\testexec dstr
call %tstbat%\testexec fnredef
call %tstbat%\testexec builtin
call %tstbat%\testexec undo
call %tstbat%\testexec gotofin

rem "Make" tests

call %tstbat%\testmake -nodef extern extern1 extern2 extern3
call %tstbat%\testmake -nodef extfunc extfunc1 extfunc2
call %tstbat%\testmake -nodef objmod objmod1 objmod2 objmod3
call %tstbat%\testmake -nodef objrep objrep1 objrep2
call %tstbat%\testmake -nodef funcrep funcrep1 funcrep2
call %tstbat%\testmake -nodef catch catch
call %tstbat%\testmake -nodef save save
call %tstbat%\testmake objloop objloop
call %tstbat%\testmake -nodef html html
call %tstbat%\testmake -nodef addlist addlist
call %tstbat%\testmake -nodef conflict conflict1 conflict2
call %tstbat%\testmake -pre vocext vocext1 vocext2 reflect
call %tstbat%\testmake -nodef listpar listpar
call %tstbat%\testmake -nodef arith arith
call %tstbat%\testmake bignum bignum
call %tstbat%\testmake bignum2 bignum2
call %tstbat%\testmake unicode unicode
call %tstbat%\testmake -nodef gram2 tok gram2
rem REMOVED - OBSOLETE call %tstbat%\testmake array array
call %tstbat%\testmake anon anon
call %tstbat%\testmake isin isin
call %tstbat%\testmake htmlify htmlify
call %tstbat%\testmake listprop listprop
call %tstbat%\testmake foreach foreach
call %tstbat%\testmake vector vector
call %tstbat%\testmake vector2 vector2
call %tstbat%\testmake lclprop lclprop
call %tstbat%\testmake lookup lookup
call %tstbat%\testmake propaddr propaddr
call %tstbat%\testmake funcparm funcparm
call %tstbat%\testmake newprop newprop
call %tstbat%\testmake anonobj anonobj
call %tstbat%\testmake nested nested
call %tstbat%\testmake badnest badnest
call %tstbat%\testmake varmac varmac
call %tstbat%\testmake -debug stack stack reflect
call %tstbat%\testmake -pre targprop targprop reflect
call %tstbat%\testmake -pre clone clone reflect
call %tstbat%\testmake anonvarg anonvarg
call %tstbat%\testmake inh_next inh_next
call %tstbat%\testmake multidyn multidyn

rem  These tests require running preinit (testmake normally suppresses it)
call %tstbat%\testmake -pre vec_pre vec_pre
call %tstbat%\testmake -pre symtab symtab
call %tstbat%\testmake -pre enumprop enumprop
call %tstbat%\testmake -pre modtobj modtobj
call %tstbat%\testmake -pre undef undef
call %tstbat%\testmake -pre undef2 undef2
call %tstbat%\testmake -pre multimethod_dynamic multimethod multmeth
call %tstbat%\testmake -pre multimethod_static -DMULTMETH_STATIC_INHERITED multimethod multmeth

rem  ITER does a save/restore test
call %tstbat%\testmake iter iter
call %tstbat%\testrestore iter2 iter

rem  "Preinit" tests
call %tstbat%\testpre preinit

rem  Resource file tests
call %tstbat%\testres resfile -res -add "test\data\resfile.dat=testres.txt"

rem  Starter game tests
call %tstbat%\testsample startI3
call %tstbat%\testsample startA3
call %tstbat%\testsample sample

rem  Return to Ditch Day tests
call %tstbat%\testditch m_walkthru ditch3_walkthru

@echo.

if exist %tstout%\*.suc     dir %tstout%\*.suc
if exist %tstout%\*.dif     dir %tstout%\*.dif

if exist %tstout%\*.log goto found_diffs
echo *** SUCCESS ***
goto done

:found_diffs
echo *** DIFFERENCE FILES FOUND ***

:done
echo.
