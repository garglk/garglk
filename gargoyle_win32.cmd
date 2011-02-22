@ECHO OFF
echo Packaging Gargoyle binaries ...
jam -sOS=MINGW -sCC=gcc -sC++=g++ install
makensis.exe installer.nsi
echo ... finished packaging!
pause
exit /b