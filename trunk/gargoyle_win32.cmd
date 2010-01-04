@ECHO OFF
echo Packaging Gargoyle binaries ...
jam -sOS=MINGW install
makensis.exe installer.nsi
echo ... finished packaging!
pause
exit /b