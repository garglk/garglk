@ECHO OFF
echo Packaging Gargoyle binaries ...
jam -sOS=MINGW -sCC=gcc -sCXX=g++ install
makensis.exe installer.nsi
echo ... finished packaging!
pause
exit /b