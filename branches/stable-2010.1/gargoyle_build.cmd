@echo off
echo Building Gargoyle for Windows ...
pushd %0\..
jam -sOS=MINGW
echo ... finished building!
pause
exit /b