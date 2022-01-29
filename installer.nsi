;
; Gargoyle NSIS 2 installer script
;

; The FontName plugin is ANSI
Unicode false

!include x64.nsh

Name "Gargoyle"
InstallDirRegKey HKLM "Software\Tor Andersson\Gargoyle\Install" "Directory"
SetCompressor lzma

!define /file VERSION "VERSION"

!if "$%GARGOYLE_ARCH%" == "x86_64"
    InstallDir $PROGRAMFILES64\Gargoyle
    OutFile "gargoyle-${VERSION}-windows-x86_64.exe"
!else
    InstallDir $PROGRAMFILES\Gargoyle
    OutFile "gargoyle-${VERSION}-windows-i686.exe"
!endif

;
; The required plugins
;

!define MULTIUSER_EXECUTIONLEVEL Admin
!include MultiUser.nsh
!include MUI.nsh
!include Unix2DOS.nsh
!include WinMessages.nsh

; From https://nsis.sourceforge.io/FontName_plug-in
!include FontReg.nsh
!include FontName.nsh

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
	; Elevate rights
    !insertmacro MULTIUSER_INIT

    !if "$%GARGOYLE_ARCH%" == "x86_64"
        SetRegView 64
    !endif

    SetOutPath $INSTDIR

    File "build\dist\*.exe"
    File "build\dist\*.dll"
    File "licenses\*.txt"
    File "fonts\*.ttf"
    File "/oname=garglk.ini.tmp" "garglk\garglk.ini"
    Push "garglk.ini.tmp"
    Push "garglk.ini"
    Call unix2dos

    CreateDirectory $INSTDIR\plugins\platforms
    SetOutPath $INSTDIR\plugins\platforms
    File "build\dist\plugins\platforms\*"

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

    ; Associate file types except dat because it's too generic
    WriteRegStr HKCR ".l9"  "" "Gargoyle.Story"
    WriteRegStr HKCR ".sna"  "" "Gargoyle.Story"
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
    WriteRegStr HKCR ".saga" "" "Gargoyle.Story"

    WriteRegStr HKCR "Gargoyle.Story" "" "Interactive Fiction Story File"
    WriteRegStr HKCR "Gargoyle.Story\DefaultIcon" "" "$INSTDIR\gargoyle.exe,0"
    WriteRegStr HKCR "Gargoyle.Story\shell" "" "open"
    WriteRegStr HKCR "Gargoyle.Story\shell\open" "" "Play"
    WriteRegStr HKCR "Gargoyle.Story\shell\open\command" "" "$INSTDIR\gargoyle.exe $\"%1$\""

    WriteRegStr HKCR "garglk" "" "URL:Gargoyle File Protocol"
    WriteRegStr HKCR "garglk" "URL Protocol" ""
    WriteRegStr HKCR "garglk\shell\open\command" "" "$INSTDIR\gargoyle.exe $\"%1$\""

SectionEnd

;
; The uninstallation script
;

Section "Uninstall"
	; Elevate rights
	!insertmacro MULTIUSER_UNINIT

    DeleteRegKey HKCR "Gargoyle.Story"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Gargoyle"
    DeleteRegKey HKLM "Software\Tor Andersson\Gargoyle"

    Delete $INSTDIR\*.exe
    Delete $INSTDIR\*.dll
    Delete $INSTDIR\*.txt
    Delete $INSTDIR\*.ttf
    Delete $INSTDIR\plugins\platforms\*.*
    RMDir $INSTDIR\plugins\platforms
    RMDir $INSTDIR\plugins

    ; Remove shortcuts, if any
    !insertmacro MUI_STARTMENU_GETFOLDER Application $R0
    Delete "$SMPROGRAMS\$R0\*.*"

    RMDir "$SMPROGRAMS\Gargoyle"
    RMDir "$INSTDIR"

    Delete "$FONTS\Gargoyle-Mono-Bold-Italic.ttf"
    Delete "$FONTS\Gargoyle-Mono-Bold.ttf"
    Delete "$FONTS\Gargoyle-Mono-Italic.ttf"
    Delete "$FONTS\Gargoyle-Mono.ttf"
    Delete "$FONTS\Gargoyle-Serif-Bold-Italic.ttf"
    Delete "$FONTS\Gargoyle-Serif-Bold.ttf"
    Delete "$FONTS\Gargoyle-Serif-Italic.ttf"
    Delete "$FONTS\Gargoyle-Serif.ttf"
SectionEnd

Section "Fonts"
    StrCpy $FONT_DIR $FONTS

    !insertmacro InstallTTFFont "fonts\Gargoyle-Mono-Bold-Italic.ttf"
    !insertmacro InstallTTFFont "fonts\Gargoyle-Mono-Bold.ttf"
    !insertmacro InstallTTFFont "fonts\Gargoyle-Mono-Italic.ttf"
    !insertmacro InstallTTFFont "fonts\Gargoyle-Mono.ttf"
    !insertmacro InstallTTFFont "fonts\Gargoyle-Serif-Bold-Italic.ttf"
    !insertmacro InstallTTFFont "fonts\Gargoyle-Serif-Bold.ttf"
    !insertmacro InstallTTFFont "fonts\Gargoyle-Serif-Italic.ttf"
    !insertmacro InstallTTFFont "fonts\Gargoyle-Serif.ttf"

    SendMessage ${HWND_BROADCAST} ${WM_FONTCHANGE} 0 0 /TIMEOUT=5000
SectionEnd
