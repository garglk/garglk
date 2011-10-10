@echo off
echo Building Gargoyle for Windows ...
pushd %0\..
jam -sOS=MINGW -sCC=gcc -sCXX=g++ -j%NUMBER_OF_PROCESSORS%
echo ... finished building!
pause
exit /b
