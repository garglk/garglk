@echo off

"\Program Files\Info-ZIP\Zip\zip" -j \Temp\MagneticDOS.zip COPYING Dos\*.exe Dos\*.fnt Dos\magnetic.txt

"\Program Files\Info-ZIP\Zip\zip" -j \Temp\MagneticDOS32.zip COPYING Dos32\magnetic.exe Dos32\*.fnt
"\Program Files\Info-ZIP\Zip\zip" -j \Temp\MagneticDOS32.zip Dos32\allegro.cfg Dos32\keyboard.dat Dos32\magnetic.txt

"\Program Files\Info-ZIP\Zip\zip" -j \Temp\MagneticWin.zip COPYING Win\Release\*.exe Win\Release\*.chm

"\Program Files\Info-ZIP\Zip\zip" \Temp\MagneticSrc.zip COPYING MakeDist.bat Generic\* Scripts\* Amiga\*
"\Program Files\Info-ZIP\Zip\zip" \Temp\MagneticSrc.zip Dos\*.c Dos\makefile Dos\bcc.cfg Dos\build.txt
"\Program Files\Info-ZIP\Zip\zip" \Temp\MagneticSrc.zip Dos32\allegro.c Dos32\makefile
"\Program Files\Info-ZIP\Zip\zip" -r \Temp\MagneticSrc.zip Win\* -x Win\Release\*
pushd \Programs
"\Program Files\Info-ZIP\Zip\zip" \Temp\MagneticSrc.zip Libraries\mfc\BCMenu.*
"\Program Files\Info-ZIP\Zip\zip" \Temp\MagneticSrc.zip Libraries\mfc\ColourButton.*
popd

"\Program Files\Info-ZIP\Zip\zip" \Temp\MagneticSrc.zip Glk\* Gtk\*

