@echo off
@echo.

call win32\test\test_env

rem Delete old log, object, symbol, image files, and test directories
echo Cleaning up old test output files...
del %tstout%\*.dif
del %tstout%\*.suc
del %tstout%\*.log
del %tstout%\*.t3s
del %tstout%\*.t3o
del %tstout%\*.t3
rmdir /s /q %tstout%\dirtest
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
call %tstbat%\testexec -cp latin1 builtin
call %tstbat%\testexec undo
call %tstbat%\testexec gotofin

rem "Make" tests

call %tstbat%\testmake -nodef extern extern1 extern2 extern3
call %tstbat%\testmake -nodef extfunc extfunc1 extfunc2
call %tstbat%\testmake -nodef objmod objmod1 objmod2 objmod3
call %tstbat%\testmake -nodef objrep objrep1 objrep2
call %tstbat%\testmake -nodef funcrep funcrep1 funcrep2
call %tstbat%\testmake -nodef catch catch
call %tstbat%\testmake except except
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
call %tstbat%\testmake vector3 vector3
call %tstbat%\testmake lclprop lclprop
call %tstbat%\testmake lookup lookup
call %tstbat%\testmake lookup2 lookup2
call %tstbat%\testmake lookup3 lookup3
call %tstbat%\testmake lookupdef lookupdef
call %tstbat%\testmake strbuf strbuf
call %tstbat%\testrun strbuf_restore strbuf -restore
call %tstbat%\testmake propaddr propaddr
call %tstbat%\testmake funcparm funcparm
call %tstbat%\testmake newprop newprop
call %tstbat%\testmake anonobj anonobj
call %tstbat%\testmake anonfunc anonfunc
call %tstbat%\testmake nested nested
call %tstbat%\testmake badnest badnest
call %tstbat%\testmake varmac varmac
call %tstbat%\testmake setsc setsc
call %tstbat%\testmake substr substr
call %tstbat%\testmake -debug stack stack reflect
call %tstbat%\testmake -pre targprop targprop reflect
call %tstbat%\testmake -pre clone clone reflect
call %tstbat%\testmake anonvarg anonvarg
call %tstbat%\testmake inh_next inh_next
call %tstbat%\testmake multidyn multidyn
call %tstbat%\testmake opoverload opoverload
call %tstbat%\testmake -script -nodef inkey inkey
call %tstbat%\testmake -nodef rand rand
call %tstbat%\testmake rand3 rand3
call %tstbat%\testmake strcomp2 strcomp2
call %tstbat%\testmake -cp latin1 strcomp3 strcomp3
call %tstbat%\testmake findreplace findreplace
call %tstbat%\testmake findall findall
call %tstbat%\testmake rexreplace rexreplace
call %tstbat%\testmake rexassert rexassert
call %tstbat%\testmake constregex constregex
call %tstbat%\testmake hashes hashes
call %tstbat%\testmake join join
call %tstbat%\testmake -norun -pre dyncomp dyncomp dynfunc
call %tstbat%\testrun dyncomp_save dyncomp -save dyncomp.t3v
call %tstbat%\testrun dyncomp_restore dyncomp -restore dyncomp.t3v
call %tstbat%\testmake asi asi
call %tstbat%\testmake spec2html spec2html
call %tstbat%\testmake spec2text spec2text
call %tstbat%\testmake propptr propptr
call %tstbat%\testmake -pre printexpr printexpr dynfunc
call %tstbat%\testmake -pre dynctx dynctx dynfunc
call %tstbat%\testmake -cp cp1252 -pre bytarr bytarr
call %tstbat%\testmake -cp cp1252 -pre bytarr2 bytarr2
call %tstbat%\testmake sprintf sprintf
call %tstbat%\testmake split split
call %tstbat%\testmake shr shr
call %tstbat%\testmake ifnil ifnil
call %tstbat%\testmake testaddr1 testaddr1 testaddr1b
call %tstbat%\testmake testaddr2 testaddr2
call %tstbat%\testmake testaddr3 testaddr3
call %tstbat%\testmake testaddr4 testaddr4
call %tstbat%\testmake strtpl strtpl
call %tstbat%\testmake listminmax listminmax
call %tstbat%\testmake defined_test1 defined
call %tstbat%\testmake defined_test2 defined defined2
call %tstbat%\testmake datatypexlat datatypexlat
call %tstbat%\testmake embedfmt embedfmt
call %tstbat%\testmake dirtest dirtest file
call %tstbat%\testmake date date
call %tstbat%\testmake datefmt datefmt
call %tstbat%\testmake dateprs dateprs
call %tstbat%\testmake -cp latin1 datelocale datelocale
call %tstbat%\testmake -norun -pre datesave datesave
call %tstbat%\testrun datesave_s datesave save
call %tstbat%\testrun datesave_r datesave restore
call %tstbat%\testmake testov testov
call %tstbat%\testmake vecmod vecmod
call %tstbat%\testmake listmod listmod
call %tstbat%\testmake idxov idxov
call %tstbat%\testmake -norun floatfolderr floatfolderr
call %tstbat%\testmake -pre floatfold floatfold reflect
call %tstbat%\testmake -pre tostring tostring reflect
call %tstbat%\testmake -pre match match
call %tstbat%\testmake -pre inlineobj inlineobj dynfunc
call %tstbat%\testmake newline_spacing newline_spacing
call %tstbat%\testmake nested_embed nested_embed
call %tstbat%\testmake nested_embed_err nested_embed_err

rem  These tests require running preinit (testmake normally suppresses it)
call %tstbat%\testmake -pre vec_pre vec_pre
call %tstbat%\testmake -pre symtab symtab
call %tstbat%\testmake -pre enumprop enumprop
call %tstbat%\testmake -pre modtobj modtobj
call %tstbat%\testmake -pre undef undef
call %tstbat%\testmake -pre undef2 undef2
call %tstbat%\testmake -pre multimethod_dynamic multimethod multmeth
call %tstbat%\testmake -pre multimethod_static -DMULTMETH_STATIC_INHERITED multimethod multmeth
call %tstbat%\testmake -pre multimethod_dynamic2 multimethod multimethod2 multmeth
call %tstbat%\testmake -pre multimethod_static2 -DMULTMETH_STATIC_INHERITED multimethod multimethod2 multmeth
call %tstbat%\testmake -pre namedparam namedparam multmeth
call %tstbat%\testmake -pre optargs optargs
call %tstbat%\testmake -pre optargs_err optargs_err
call %tstbat%\testmake -pre optargs_err2 optargs_err2
call %tstbat%\testmake -pre bifptr bifptr dynfunc
call %tstbat%\testmake -pre setmethod setmethod dynfunc
call %tstbat%\testmake -pre lclvars lclvars lclvars2 reflect dynfunc
call %tstbat%\testmake -pre dynamicGrammar dynamicGrammar tok dynfunc gramprod
call %tstbat%\testmake -pre triplequote triplequote
call %tstbat%\testmake -pre newembed newembed
call %tstbat%\testmake -pre newembederr newembederr

call %tstbat%\testpack packfile
call %tstbat%\testpack packfloats
call %tstbat%\testmake packstr packstr
call %tstbat%\testmake packarr packarr

rem  ITER does a save/restore test
call %tstbat%\testmake iter iter
call %tstbat%\testrestore iter2 iter

rem  "Preinit" tests
call %tstbat%\testpre preinit

rem  Resource file tests
call %tstbat%\testres resfile -res -add "test\data\resfile.dat=testres.txt"

rem  Starter game tests
call %tstbat%\testsample /Adv3 startI3
call %tstbat%\testsample /Adv3 startA3
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
