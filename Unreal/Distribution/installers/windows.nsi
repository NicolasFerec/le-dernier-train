Unicode True
!include "MUI2.nsh"
!include "x64.nsh"

Name "Le Dernier Train"
OutFile "${SETUP_FILE}"
InstallDir "$LOCALAPPDATA\Programs\Le Dernier Train Unreal"
RequestExecutionLevel user
SetCompressor lzma
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\LeDernierTrainUnreal"
!define MUI_ICON "${TRAIN_ICON}"
!define MUI_UNICON "${TRAIN_ICON}"
!define MUI_LANGDLL_REGISTRY_ROOT HKCU
!define MUI_LANGDLL_REGISTRY_KEY "${UNINSTALL_KEY}"
!define MUI_LANGDLL_REGISTRY_VALUENAME "InstallerLanguage"
!define MUI_FINISHPAGE_RUN "$INSTDIR\LeDernierTrainLauncher.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "French"
!insertmacro MUI_LANGUAGE "English"

LangString StartFolder ${LANG_FRENCH} "Jeux"
LangString StartFolder ${LANG_ENGLISH} "Games"
LangString CloseGame ${LANG_FRENCH} "Ferme Le Dernier Train et son lanceur, puis réessaie."
LangString CloseGame ${LANG_ENGLISH} "Close Le Dernier Train and its launcher, then try again."
LangString Requires64Bit ${LANG_FRENCH} "Cet installateur nécessite Windows 64 bits."
LangString Requires64Bit ${LANG_ENGLISH} "This installer requires 64-bit Windows."

Function .onInit
    SetShellVarContext current
    SetRegView 64
    !insertmacro MUI_LANGDLL_DISPLAY
    ${IfNot} ${RunningX64}
        MessageBox MB_ICONSTOP "$(Requires64Bit)"
        Abort
    ${EndIf}
FunctionEnd

Function un.onInit
    SetShellVarContext current
    SetRegView 64
    !insertmacro MUI_UNGETLANGUAGE
FunctionEnd

Section "Le Dernier Train"
    SetOutPath "$INSTDIR"
    ClearErrors
    File /oname=LeDernierTrainLauncher.exe "${TRAIN_LAUNCHER}"
    IfErrors 0 launcherInstalled
        MessageBox MB_ICONSTOP "$(CloseGame)"
        Abort
    launcherInstalled:
    File /oname=Train.ico "${TRAIN_ICON}"
    SetCompress off
    File /oname=bootstrap.zip "${TRAIN_BUNDLE}"
    SetCompress auto
    File /oname=bootstrap.json "${TRAIN_BUNDLE_META}"
    Delete "$SMPROGRAMS\Jeux\Le Dernier Train.lnk"
    Delete "$SMPROGRAMS\Games\Le Dernier Train.lnk"
    CreateDirectory "$SMPROGRAMS\$(StartFolder)"
    CreateShortCut "$SMPROGRAMS\$(StartFolder)\Le Dernier Train.lnk" "$INSTDIR\LeDernierTrainLauncher.exe" "" "$INSTDIR\Train.ico"
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "Le Dernier Train"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\Train.ico"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section "Uninstall"
    IfFileExists "$INSTDIR\LeDernierTrainLauncher.exe" 0 launcherRemoved
    ClearErrors
    Delete "$INSTDIR\LeDernierTrainLauncher.exe"
    IfErrors 0 launcherRemoved
        MessageBox MB_ICONSTOP "$(CloseGame)"
        Abort
    launcherRemoved:
    Delete "$SMPROGRAMS\Jeux\Le Dernier Train.lnk"
    Delete "$SMPROGRAMS\Games\Le Dernier Train.lnk"
    RMDir "$SMPROGRAMS\Jeux"
    RMDir "$SMPROGRAMS\Games"
    DeleteRegKey HKCU "${UNINSTALL_KEY}"
    RMDir /r "$INSTDIR\versions"
    RMDir /r "$INSTDIR\incoming"
    Delete "$INSTDIR\Train.ico"
    Delete "$INSTDIR\current-stable.json"
    Delete "$INSTDIR\current-dev.json"
    Delete "$INSTDIR\bootstrap.zip"
    Delete "$INSTDIR\bootstrap.json"
    Delete "$INSTDIR\.lock"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
SectionEnd
