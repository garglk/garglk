@echo off

"\Program Files\Info-ZIP\Zip\zip" -j \Temp\Level9_Dos16.zip COPYING level9.txt DOS/level9.exe

"\Program Files\Info-ZIP\Zip\zip" -j \Temp\Level9_Dos32.zip COPYING level9.txt
"\Program Files\Info-ZIP\Zip\zip" -j \Temp\Level9_Dos32.zip DOS32/level9.exe DOS32/allegro.cfg DOS32/keyboard.dat
"\Program Files\Info-ZIP\Zip\zip" -j \Temp\Level9_Dos32.zip \DosApps\DJGPP\bin\cwsdpmi.exe

"\Program Files\Info-ZIP\Zip\zip" -j \Temp\Level9_Win32.zip COPYING Win/Release/Level9.exe Win/Release/Level9.hlp

"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9_Source.zip COPYING *.c *.h *.bat *.txt -x todo.txt
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9_Source.zip Amiga/*
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9_Source.zip DOS/* -x DOS/*.exe
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9_Source.zip DOS32/* -x DOS32/*.exe
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9_Source.zip Glk/*
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9_Source.zip Unix/*
"\Program Files\Info-ZIP\Zip\zip" \Temp\Level9_Source.zip Win/* Win/Classlib/*

