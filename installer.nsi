;
; Gargoyle NSIS 2 installer script
;

RequestExecutionLevel admin
!include "MUI.nsh"
!include FontRegAdv.nsh
!include FontName.nsh


Name "Gargoyle"
OutFile "gargoyle-2010.1-windows.exe"
InstallDir $PROGRAMFILES\Gargoyle
InstallDirRegKey HKLM "Software\Tor Andersson\Gargoyle\Install" "Directory"

SetCompressor lzma

;
; The installer theme
;

!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\orange-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\orange-uninstall.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Wizard\orange.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${NSISDIR}\Contrib\Graphics\Header\orange.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "${NSISDIR}\Contrib\Graphics\Header\orange-uninstall.bmp"

;
; Define which pages we want
;

!define MUI_ABORTWARNING

Var SMFOLDER
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "Gargoyle"
!define MUI_STARTMENUPAGE_REGISTRY_ROOT HKLM
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Tor Andersson\Gargoyle\Install"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu"

; !insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "License.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU Application $SMFOLDER
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

;
; The installation script
;

Section "DoInstall"
    SetOutPath $INSTDIR

    File "build\dist\*.exe"
    File "build\dist\*.dll"
    File "licenses\*.txt"
    File "garglk\garglk.ini"
    
    SetOutPath $INSTDIR\etc\fonts
    File "build\dist\etc\fonts\fonts.conf"
    File "build\dist\etc\fonts\fonts.dtd"

    WriteUninstaller "uninstall.exe"

    !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
	CreateDirectory "$SMPROGRAMS\$SMFOLDER"
	CreateShortCut "$SMPROGRAMS\$SMFOLDER\Gargoyle.lnk" "$INSTDIR\gargoyle.exe" "" "$INSTDIR\gargoyle.exe" 0
	CreateShortCut "$SMPROGRAMS\Gargoyle\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
    !insertmacro MUI_STARTMENU_WRITE_END

    ; Write the installation path into the registry
    WriteRegStr HKLM "Software\Tor Andersson\Gargoyle" "Directory" "$INSTDIR"

    ; Write the uninstall keys for Windows
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gargoyle" "DisplayName" "Gargoyle"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gargoyle" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gargoyle" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gargoyle" "NoRepair" 1

    ; Associate file types except dat and sna because they're too generic
    WriteRegStr HKCR ".l9"  "" "Gargoyle.Story"
    WriteRegStr HKCR ".t3"  "" "Gargoyle.Story"
    WriteRegStr HKCR ".z3"  "" "Gargoyle.Story"
    WriteRegStr HKCR ".z4"  "" "Gargoyle.Story"
    WriteRegStr HKCR ".z5"  "" "Gargoyle.Story"
    WriteRegStr HKCR ".z6"  "" "Gargoyle.Story"
    WriteRegStr HKCR ".z8"  "" "Gargoyle.Story"
    WriteRegStr HKCR ".taf" "" "Gargoyle.Story"
    WriteRegStr HKCR ".agx" "" "Gargoyle.Story"
    WriteRegStr HKCR ".acd" "" "Gargoyle.Story"
    WriteRegStr HKCR ".a3c" "" "Gargoyle.Story"
    WriteRegStr HKCR ".asl" "" "Gargoyle.Story"
    WriteRegStr HKCR ".cas" "" "Gargoyle.Story"
    WriteRegStr HKCR ".ulx" "" "Gargoyle.Story"
    WriteRegStr HKCR ".hex" "" "Gargoyle.Story"
    WriteRegStr HKCR ".jacl" "" "Gargoyle.Story"
    WriteRegStr HKCR ".j2" "" "Gargoyle.Story"
    WriteRegStr HKCR ".gam" "" "Gargoyle.Story"
    WriteRegStr HKCR ".mag" "" "Gargoyle.Story"
    WriteRegStr HKCR ".blb" "" "Gargoyle.Story"
    WriteRegStr HKCR ".glb" "" "Gargoyle.Story"
    WriteRegStr HKCR ".zlb" "" "Gargoyle.Story"
    WriteRegStr HKCR ".blorb" "" "Gargoyle.Story"
    WriteRegStr HKCR ".gblorb" "" "Gargoyle.Story"
    WriteRegStr HKCR ".zblorb" "" "Gargoyle.Story"
    WriteRegStr HKCR ".d$$$$" "" "Gargoyle.Story"

    WriteRegStr HKCR "Gargoyle.Story" "" "Interactive Fiction Story File"
    WriteRegStr HKCR "Gargoyle.Story\DefaultIcon" "" "$INSTDIR\gargoyle.exe,1"
    WriteRegStr HKCR "Gargoyle.Story\shell" "" "open"
    WriteRegStr HKCR "Gargoyle.Story\shell\open" "" "Play"
    WriteRegStr HKCR "Gargoyle.Story\shell\open\command" "" "$INSTDIR\gargoyle.exe $\"%1$\""

SectionEnd

Section "Fonts"
    StrCpy $FONT_DIR $FONTS
    !insertmacro InstallTTF "fonts\LuxiMono-Regular.ttf"
    !insertmacro InstallTTF "fonts\LuxiMono-Bold.ttf"
    !insertmacro InstallTTF "fonts\LuxiMono-Oblique.ttf"
    !insertmacro InstallTTF "fonts\LuxiMono-BoldOblique.ttf"
    !insertmacro InstallTTF "fonts\CharterBT-Roman.ttf"
    !insertmacro InstallTTF "fonts\CharterBT-Bold.ttf"
    !insertmacro InstallTTF "fonts\CharterBT-Italic.ttf"
    !insertmacro InstallTTF "fonts\CharterBT-BoldItalic.ttf"
    !insertmacro InstallTTF "fonts\LiberationMono-Regular.ttf"
    !insertmacro InstallTTF "fonts\LiberationMono-Bold.ttf"
    !insertmacro InstallTTF "fonts\LiberationMono-Italic.ttf"
    !insertmacro InstallTTF "fonts\LiberationMono-BoldItalic.ttf"
    !insertmacro InstallTTF "fonts\LinLibertine_Re-4.4.1.ttf"
    !insertmacro InstallTTF "fonts\LinLibertine_Bd-4.1.0.ttf"
    !insertmacro InstallTTF "fonts\LinLibertine_It-4.0.6.ttf"
    !insertmacro InstallTTF "fonts\LinLibertine_BI-4.0.5.ttf"
    SendMessage ${HWND_BROADCAST} ${WM_FONTCHANGE} 0 0 /TIMEOUT=5000

SectionEnd

;
; The uninstallation script
;

Section "Uninstall"

    DeleteRegKey HKCR "Gargoyle.Story"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gargoyle"
    DeleteRegKey HKLM "Software\Tor Andersson\Gargoyle"

    Delete $INSTDIR\*.exe
    Delete $INSTDIR\*.dll
    Delete $INSTDIR\*.txt

    ; Remove shortcuts, if any
    !insertmacro MUI_STARTMENU_GETFOLDER Application $R0
    Delete "$SMPROGRAMS\$R0\*.*"

    RMDir "$SMPROGRAMS\Gargoyle"
    RMDir "$INSTDIR"

SectionEnd

