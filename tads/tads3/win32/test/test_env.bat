rem set up the test environment variables

set tstbin=%cd%\..\build
set tstbat=%cd%\win32\test
set tstroot=%cd%\test
set tstdat=%tstroot%\data
set tstout=%tstroot%\out
set tstlog=%tstroot%\log

set tstinc=%t3tstinc%
set tstlib=%t3tstlib%
set tstsamp=%t3tstsamp%

set tstditch=%cd%\..\games\ditch3

if "%tstinc%" == "" set tstinc=%cd%\include
if "%tstlib%" == "" set tstlib=%cd%\lib
if "%tstsamp%" == "" set tstsamp=%cd%\lib\adv3\samples

