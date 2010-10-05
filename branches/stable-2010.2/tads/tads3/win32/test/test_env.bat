rem set up the test environment variables

set tstbin=..\build
set tstbat=.\win32\test
set tstdat=test\data
set tstout=test\out
set tstlog=test\log

set tstinc=%t3tstinc%
set tstlib=%t3tstlib%
set tstsamp=%t3tstsamp%

set tstditch=\tads\games\ditch3

if "%tstinc%" == "" set tstinc=.\include
if "%tstlib%" == "" set tstlib=.\lib
if "%tstsamp%" == "" set tstsamp=.\samples

