!define PRODUCT_NAME "IzakayaCheater"
!define PRODUCT_VERSION "0.2.0"
!define PRODUCT_PUBLISHER "lbw14514"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "IzakayaCheater_v${PRODUCT_VERSION}_Setup.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
RequestExecutionLevel admin

Section "Install"
  SetOutPath "$INSTDIR"
  File "release\IzakayaCheater.exe"
  File "release\*.dll"
  File "release\README.md"
  File "release\LICENSE"
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\IzakayaCheater.lnk" "$INSTDIR\IzakayaCheater.exe"
  CreateShortCut "$DESKTOP\IzakayaCheater.lnk" "$INSTDIR\IzakayaCheater.exe"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}" "UninstallString" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\*.*"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\*.*"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  Delete "$DESKTOP\IzakayaCheater.lnk"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
SectionEnd
