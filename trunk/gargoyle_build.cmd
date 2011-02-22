@echo off
echo Building Gargoyle for Windows ...
pushd %0\..
jam -sOS=MINGW -sCC=gcc -sC++=g++
echo ... finished building!
pause
exit /b