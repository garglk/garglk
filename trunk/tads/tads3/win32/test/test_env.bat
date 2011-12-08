rem set up the test environment variables

set tstbin=%cd%\..\build
set tstbat=%cd%\.\win32\test
set tstdat=%cd%\test\data
set tstout=%cd%\test\out
set tstlog=%cd%\test\log

set tstinc=%t3tstinc%
set tstlib=%t3tstlib%
set tstsamp=%t3tstsamp%

set tstditch=%cd%\..\games\ditch3

if "%tstinc%" == "" set tstinc=%cd%\include
if "%tstlib%" == "" set tstlib=%cd%\lib
if "%tstsamp%" == "" set tstsamp=%cd%\samples

