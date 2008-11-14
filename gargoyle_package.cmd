@ECHO OFF
echo Packaging Gargoyle binaries ...
jam -sOS=MINGW install
copy support\sdl-1.2.13\SDL_sound.dll build\dist
copy support\sdl-1.2.13\SDL_mixer.dll build\dist
copy support\sdl-1.2.13\SDL.dll build\dist
copy support\sdl-1.2.13\smpeg.dll build\dist
copy support\sdl-1.2.13\libogg-0.dll build\dist
copy support\sdl-1.2.13\libvorbis-0.dll build\dist
copy support\sdl-1.2.13\libvorbisfile-3.dll build\dist
makensis.exe installer.nsi
echo ... finished packaging!
pause
exit /b