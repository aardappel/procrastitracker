
!include "MUI.nsh"
!define MUI_FINISHPAGE_RUN "$INSTDIR\ProcrastiTracker.exe"
!define MUI_HEADERIMAGE
/*
!define MUI_HEADERIMAGE_BITMAP "procrastitracker\dot3inst.bmp"
*/
/*
doesn't show?
!define MUI_HEADERIMAGE_UNBITMAP "procrastitracker\dot3inst.bmp"
*/

Name "ProcrastiTracker"

OutFile "ProcrastiTracker_Setup.exe"

XPStyle on

InstallDir $PROGRAMFILES\ProcrastiTracker

InstallDirRegKey HKLM "Software\ProcrastiTracker" "Install_Dir"

SetCompressor /SOLID lzma
XPStyle on

Page components #"" ba ""
Page directory 
Page instfiles 
!insertmacro MUI_PAGE_FINISH

UninstPage uninstConfirm 
UninstPage instfiles 

!insertmacro MUI_LANGUAGE "English"

/*
AddBrandingImage top 65

Function ba
	File procrastitracker\dot3.bmp
  SetBrandingImage procrastitracker\dot3.bmp
FunctionEnd

Function un.ba
  SetBrandingImage procrastitracker\dot3.bmp
FunctionEnd
*/

Function .onInit
  FindWindow $0 "PROCRASTITRACKER" ""
  StrCmp $0 0 continueInstall
    MessageBox MB_ICONSTOP|MB_OK "Procrastitracker is already running, please close it and try again."
    Abort
  continueInstall:
FunctionEnd

Function un.onInit
  FindWindow $0 "PROCRASTITRACKER" ""
  StrCmp $0 0 continueInstall
    MessageBox MB_ICONSTOP|MB_OK "Procrastitracker is still running, please close it and try again."
    Abort
  continueInstall:
FunctionEnd

Section "ProcrastiTracker (required)"

  SectionIn RO
  
  SetOutPath $INSTDIR
  
  File /r "PT\*.*"
  
  WriteRegStr HKLM SOFTWARE\ProcrastiTracker "Install_Dir" "$INSTDIR"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProcrastiTracker" "DisplayName" "ProcrastiTracker"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProcrastiTracker" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProcrastiTracker" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProcrastiTracker" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

/*
Section "Visual C++ redistributable runtime"

  ExecWait '"$INSTDIR\redist\vcredist_x86.exe"'
  
SectionEnd
*/

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\ProcrastiTracker"
  CreateDirectory "$APPDATA\procrastitrackerdbs\"
  
  SetOutPath "$INSTDIR"
  
  CreateShortCut "$SMPROGRAMS\ProcrastiTracker\ProcrastiTracker.lnk" "$INSTDIR\ProcrastiTracker.exe" "" "$INSTDIR\ProcrastiTracker.exe" 0
  CreateShortCut "$SMPROGRAMS\ProcrastiTracker\Uninstall.lnk"        "$INSTDIR\uninstall.exe"        "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\ProcrastiTracker\README.lnk"           "$INSTDIR\README.html"          "" "$INSTDIR\README.html" 0
  CreateShortCut "$SMPROGRAMS\ProcrastiTracker\Database Files.lnk"   "$APPDATA\procrastitrackerdbs\" "" "$APPDATA\procrastitrackerdbs\" 0
  
  CreateShortCut "$SMSTARTUP\ProcrastiTracker.lnk"                   "$INSTDIR\ProcrastiTracker.exe" "" "$INSTDIR\ProcrastiTracker.exe" 0
  
SectionEnd

Section "Uninstall"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\ProcrastiTracker"
  DeleteRegKey HKLM SOFTWARE\ProcrastiTracker

  RMDir /r "$SMPROGRAMS\ProcrastiTracker"
  Delete "$SMSTARTUP\ProcrastiTracker.lnk"
  RMDir /r "$INSTDIR"

SectionEnd
