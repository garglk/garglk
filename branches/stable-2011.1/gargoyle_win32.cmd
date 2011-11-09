@ECHO OFF
echo Packaging Gargoyle binaries ...
jam -sOS=MINGW -sCC=gcc -sCXX=g++ -j%NUMBER_OF_PROCESSORS% install
makensis.exe installer.nsi
echo ... finished packaging!
pause
exit /b