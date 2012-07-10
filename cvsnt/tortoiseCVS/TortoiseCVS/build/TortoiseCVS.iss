; TortoiseCVS - a Windows shell extension for easy version control
;
; Inno Setup install script
;
; Copyright (C) 2006 - Torsten Martinsen
; <torsten@vip.cybercity.dk> - July 2006
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation; either version 2
; of the License, or (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

[Setup]
AppID=TortoiseCVS
AppName=TortoiseCVS
AppVerName=TortoiseCVS {#TORTOISEVERSION}
AppVersion={#TORTOISEVERSION}
AppPublisherURL=http://www.tortoisecvs.org
AppSupportURL=http://www.tortoisecvs.org/support.shtml
AppUpdatesURL=http://www.tortoisecvs.org/download.shtml
DefaultDirName={pf}\TortoiseCVS
DefaultGroupName=TortoiseCVS
OutputDir=.
OutputBaseFilename=TortoiseCVS
PrivilegesRequired=admin
AlwaysRestart=no
VersionInfoVersion={#TORTOISEVERSIONNUMERIC}
; Only install on W2K or later
MinVersion=0,5
; Win64
ArchitecturesInstallIn64BitMode=x64

; Compression settings
Compression=lzma/ultra
InternalCompressLevel=ultra
SolidCompression=yes

; Some settings to avoid silly questions

; Perhaps enable this later
DirExistsWarning=no
DisableProgramGroupPage=yes
DisableReadyPage=true
DisableStartupPrompt=true
AlwaysShowComponentsList=false

UninstallDisplayIcon={app}\TortoiseAct.exe
WizardImageFile=charlie.bmp

[Languages]
Name: "en_GB"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "main"; Description: _("Main Files"); Types: full compact custom; Flags: fixed
Name: "icons"; Description: _("Custom Icons"); Types: full
Name: "icons\barracuda"; Description: "Barracuda"; Types: full
Name: "icons\bertels"; Description: "Stefan Bertels"; Types: full
Name: "icons\nikolai"; Description: "Nikolai Devereaux"; Types: full
Name: "icons\tbf"; Description: "Mathias Hasselmann"; Types: full
Name: "icons\timo"; Description: "Timo Kauppinen"; Types: full
Name: "icons\cosmin"; Description: "Cosmin Smeu"; Types: full
Name: "icons\gregsearle"; Description: "Greg Searle"; Types: full
Name: "icons\PierreOssman"; Description: "Pierre Ossman"; Types: full
Name: "icons\NG"; Description: "Nikolai Giraldo"; Types: full
Name: "icons\TomasLehuta"; Description: "Tomas Lehuta"; Types: full
Name: "icons\XPStyleRounded"; Description: _("XP-style rounded"); Types: full
Name: "icons\Classic"; Description: _("Classic TortoiseCVS"); Types: full
Name: "icons\DanAlpha"; Description: _("Daniel Jackson DanAlpha"); Types: full
Name: "icons\visli"; Description: _("Chinese characters"); Types: full
Name: "language"; Description: _("Translations"); Types: full
Name: "language\ar_TN"; Description: LANG("Arabic"); Types: full
Name: "language\ca_01"; Description: LANG("Catalan"); Types: full
Name: "language\cs_CZ"; Description: LANG("Czech"); Types: full
;Name: "language\da_DK"; Description: LANG("Danish"); Types: full
Name: "language\hu_HU"; Description: LANG("Hungarian"); Types: full
Name: "language\nb_NO"; Description: LANG("Norwegian Bokm�l"); Types: full
Name: "language\nl_NL"; Description: LANG("Dutch"); Types: full
Name: "language\de_DE"; Description: LANG("German"); Types: full
Name: "language\es_ES"; Description: LANG("Spanish"); Types: full
Name: "language\fr_FR"; Description: LANG("French"); Types: full
Name: "language\it_IT"; Description: LANG("Italian"); Types: full
Name: "language\ja_JP"; Description: LANG("Japanese"); Types: full
Name: "language\ka_GE"; Description: LANG("Georgian"); Types: full
;Name: "language\ko_KR"; Description: LANG("Korean"); Types: full
Name: "language\pl_PL"; Description: LANG("Polish"); Types: full
Name: "language\pt_BR"; Description: LANG("Portuguese"); Types: full
Name: "language\ro_RO"; Description: LANG("Romanian"); Types: full
Name: "language\ru_RU"; Description: LANG("Russian"); Types: full
Name: "language\sl_SI"; Description: LANG("Slovenian"); Types: full
Name: "language\zh_CN"; Description: LANG("Simplified Chinese [GBK]"); Types: full
Name: "language\zh_TW"; Description: LANG("Traditional Chinese [Big5]"); Types: full
;Name: "language\tr_TR"; Description: LANG("Turkish"); Types: full
Name: "pageant"; Description: _("Pageant SSH agent"); Types: full

[Files]
; These are in the build directory
Source: TortoiseCVS.Filetypes; DestDir: {app}; Flags: restartreplace uninsrestartdelete
Source: TortoiseCVSError.wav; DestDir: {app}; Flags: restartreplace uninsrestartdelete
Source: TortoiseMenus.config; DestDir: {app}; Flags: restartreplace uninsrestartdelete

; CVSNT installer
Source: ..\src\Cvsnt\{#CVSNTINSTALLER}; DestDir: {tmp}

; PUTTY binaries
Source: ..\src\putty\puttygen.exe; DestDir: {app}; Flags: restartreplace uninsrestartdelete
Source: ..\src\putty\pageant.exe; DestDir: {app}; Flags: restartreplace uninsrestartdelete; Components: pageant

; VC9 redistributables
Source: ..\src\SharedDlls\VC9RunTime\Release\VC9RunTime.msi; DestDir: {tmp}
Source: ..\src\SharedDlls\VC9RunTimeX64\Release\VC9RunTimeX64.msi; DestDir: {tmp}; Check: IsWin64

; TortoiseOverlays
Source: ..\src\SharedDlls\TortoiseOverlays-1.0.9.17375-win32.msi; DestDir: {tmp}
Source: ..\src\SharedDlls\TortoiseOverlays-1.0.9.17375-x64.msi; DestDir: {tmp}; Check: IsWin64

; GDI+ redistributable, only for Windows 2000 and NT
Source: ..\src\SharedDlls\gdiplus.dll; DestDir: {app}; OnlyBelowVersion: 0,5.01.2600; Flags: uninsrestartdelete

; Standard icons
Source: ..\src\icons\TortoiseCVS\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\TortoiseCVS; Flags: uninsneveruninstall

; Icons by Barracuda
Source: ..\src\icons\Barracuda\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\Barracuda; Components: "icons\barracuda"; Flags: uninsneveruninstall

; Icons by Stefan Bertels
Source: ..\src\icons\Bertels\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\Bertels; Components: "icons\bertels"; Flags: uninsneveruninstall

; Icons by Cosmin Smeu
Source: ..\src\icons\Cosmin\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\Cosmin; Components: "icons\cosmin"; Flags: uninsneveruninstall

; Icons by Nikolai Devereaux
Source: ..\src\icons\Nikolai\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\Nikolai; Components: "icons\nikolai"; Flags: uninsneveruninstall

; Icons by Mathias Hasselmann
Source: ..\src\icons\Tbf\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\Tbf; Components: "icons\tbf"; Flags: uninsneveruninstall

; Icons by Timo Kauppinen
Source: ..\src\icons\Timo\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\Timo; Components: "icons\timo"; Flags: uninsneveruninstall

; Icons by Greg Searle
Source: ..\src\icons\GregSearle\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\GregSearle; Components: "icons\gregsearle"; Flags: uninsneveruninstall

Source: ..\src\icons\PierreOssman\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\PierreOssman; Components: "icons\PierreOssman"; Flags: uninsneveruninstall

Source: ..\src\icons\NG\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\NG; Components: "icons\NG"; Flags: uninsneveruninstall

Source: ..\src\icons\XPStyleRounded\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\XPStyleRounded; Components: "icons\XPStyleRounded"; Flags: uninsneveruninstall

Source: ..\src\icons\Classic\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\Classic; Components: "icons\Classic"; Flags: uninsneveruninstall

Source: ..\src\icons\DanAlpha\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\DanAlpha; Components: "icons\DanAlpha"; Flags: uninsneveruninstall
Source: ..\src\icons\visli\*.ico; DestDir: {cf32}\TortoiseOverlays\icons\visli; Components: "icons\visli"; Flags: uninsneveruninstall


;;;;; x64 icons

; Standard icons
Source: ..\src\icons\TortoiseCVS\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\TortoiseCVS; Flags: uninsneveruninstall; Check: IsWin64

; Icons by Barracuda
Source: ..\src\icons\Barracuda\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\Barracuda; Components: "icons\barracuda"; Flags: uninsneveruninstall; Check: IsWin64

; Icons by Stefan Bertels
Source: ..\src\icons\Bertels\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\Bertels; Components: "icons\bertels"; Flags: uninsneveruninstall; Check: IsWin64

; Icons by Cosmin Smeu
Source: ..\src\icons\Cosmin\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\Cosmin; Components: "icons\cosmin"; Flags: uninsneveruninstall; Check: IsWin64

; Icons by Nikolai Devereaux
Source: ..\src\icons\Nikolai\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\Nikolai; Components: "icons\nikolai"; Flags: uninsneveruninstall; Check: IsWin64

; Icons by Mathias Hasselmann
Source: ..\src\icons\Tbf\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\Tbf; Components: "icons\tbf"; Flags: uninsneveruninstall; Check: IsWin64

; Icons by Timo Kauppinen
Source: ..\src\icons\Timo\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\Timo; Components: "icons\timo"; Flags: uninsneveruninstall; Check: IsWin64

; Icons by Greg Searle
Source: ..\src\icons\GregSearle\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\GregSearle; Components: "icons\gregsearle"; Flags: uninsneveruninstall; Check: IsWin64

Source: ..\src\icons\PierreOssman\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\PierreOssman; Components: "icons\PierreOssman"; Flags: uninsneveruninstall; Check: IsWin64

Source: ..\src\icons\NG\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\NG; Components: "icons\NG"; Flags: uninsneveruninstall; Check: IsWin64

Source: ..\src\icons\XPStyleRounded\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\XPStyleRounded; Components: "icons\XPStyleRounded"; Flags: uninsneveruninstall; Check: IsWin64

Source: ..\src\icons\Classic\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\Classic; Components: "icons\Classic"; Flags: uninsneveruninstall; Check: IsWin64

Source: ..\src\icons\DanAlpha\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\DanAlpha; Components: "icons\DanAlpha"; Flags: uninsneveruninstall; Check: IsWin64

Source: ..\src\icons\visli\*.ico; DestDir: {cf64}\TortoiseOverlays\icons\visli; Components: "icons\visli"; Flags: uninsneveruninstall; Check: IsWin64

; HTML etc.
Source: ..\web\ChangeLog.txt; DestDir: {app};
Source: ..\web\GPL.html; DestDir: {app};
Source: ..\web\Help.html; DestDir: {app};
Source: ..\web\astronlicense.html; DestDir: {app};
Source: ..\web\charlie.jpeg; DestDir: {app};
Source: ..\web\faq.html; DestDir: {app};
Source: ..\web\legal.html; DestDir: {app};
Source: ..\web\philosophical-gnu-sm.jpg; DestDir: {app};
Source: ..\web\tcvs.css; DestDir: {app};

; Compiled executables
Source: {#POSTINST_PATH}; DestDir: {app}; Flags: ignoreversion deleteafterinstall
Source: {#TORTOISEACT_PATH}; DestDir: {app}; Flags: restartreplace uninsrestartdelete ignoreversion
Source: {#TORTOISEPLINK_PATH}; DestDir: {app}; Flags: restartreplace uninsrestartdelete ignoreversion
Source: {#TORTOISESHELL_PATH}; DestDir: {app}; Flags: restartreplace uninsrestartdelete ignoreversion
Source: {#TORTOISESHELL64_PATH}; DestDir: {app}; DestName: TortoiseShell64.dll; Flags: restartreplace uninsrestartdelete ignoreversion; Check: IsWin64

; Helpers for installer
Source: {#TORTOISESETUPHELPER_PATH}; DestDir: {app}
Source: {#TORTOISESETUPHELPER64_PATH}; DestDir: {app}; DestName: TortoiseSetupHelper64.exe; Check: IsWin64
Source: {#RUNTIMEINSTALLER_PATH}; DestDir: {app}; Flags: dontcopy
Source: {#RUNTIMEINSTALLER64_PATH}; DestDir: {app}; DestName: RunTimeInstaller64.exe; Check: IsWin64; Flags: dontcopy

; Localization
Source: ..\..\po\TortoiseCVS\en_GB.mo; DestDir: {app}\locale\en_GB; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion
Source: ..\..\po\TortoiseCVS\ar_TN.mo; DestDir: {app}\locale\ar_TN; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\ar_TN"
Source: ..\..\po\TortoiseCVS\ca_01.mo; DestDir: {app}\locale\ca; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\ca_01"
Source: ..\..\po\TortoiseCVS\cs_CZ.mo; DestDir: {app}\locale\cs_CZ; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete; Components: "language\cs_CZ"
;Source: ..\..\po\TortoiseCVS\da_DK.mo; DestDir: {app}\locale\da_DK; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\da_DK"
Source: ..\..\po\TortoiseCVS\de_DE.mo; DestDir: {app}\locale\de_DE; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\de_DE"
Source: ..\..\po\TortoiseCVS\es_ES.mo; DestDir: {app}\locale\es_ES; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\es_ES"
Source: ..\..\po\TortoiseCVS\fr_FR.mo; DestDir: {app}\locale\fr_FR; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\fr_FR"
Source: ..\..\po\TortoiseCVS\it_IT.mo; DestDir: {app}\locale\it_IT; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete; Components: "language\it_IT"
Source: ..\..\po\TortoiseCVS\ja_JP.mo; DestDir: {app}\locale\ja_JP; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete; Components: "language\ja_JP"
Source: ..\..\po\TortoiseCVS\hu_HU.mo; DestDir: {app}\locale\hu_HU; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\hu_HU"
Source: ..\..\po\TortoiseCVS\ka_GE.mo; DestDir: {app}\locale\ka; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\ka_GE"
;Source: ..\..\po\TortoiseCVS\ko_KR.mo; DestDir: {app}\locale\ko_KR; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\ko_KR"
Source: ..\..\po\TortoiseCVS\nb_NO.mo; DestDir: {app}\locale\nb_NO; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\nb_NO"
Source: ..\..\po\TortoiseCVS\nl_NL.mo; DestDir: {app}\locale\nl_NL; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\nl_NL"
Source: ..\..\po\TortoiseCVS\pt_BR.mo; DestDir: {app}\locale\pt_BR; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\pt_BR"
Source: ..\..\po\TortoiseCVS\ro_RO.mo; DestDir: {app}\locale\ro_RO; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\ro_RO"
Source: ..\..\po\TortoiseCVS\ru_RU.mo; DestDir: {app}\locale\ru_RU; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\ru_RU"
Source: ..\..\po\TortoiseCVS\sl_SI.mo; DestDir: {app}\locale\sl_SI; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\sl_SI"
;Source: ..\..\po\TortoiseCVS\zh_CN.mo; DestDir: {app}\locale\zh_CN; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\zh_CN"
Source: ..\..\po\TortoiseCVS\zh_TW.mo; DestDir: {app}\locale\zh_TW; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\zh_TW"
;Source: ..\..\po\TortoiseCVS\tr_TR.mo; DestDir: {app}\locale\tr_TR; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\tr_TR"
Source: ..\..\po\TortoiseCVS\pl_PL.mo; DestDir: {app}\locale\pl_PL; DestName: TortoiseCVS.mo; Flags: restartreplace uninsrestartdelete ignoreversion; Components: "language\pl_PL"

; User Guide
Source: ..\..\docs\UserGuide_en.chm; DestDir: {app}; Flags: restartreplace uninsrestartdelete ignoreversion
Source: ..\..\docs\UserGuide_fr.chm; DestDir: {app}; Flags: restartreplace uninsrestartdelete ignoreversion
Source: ..\..\docs\UserGuide_cn.chm; DestDir: {app}; Flags: restartreplace uninsrestartdelete ignoreversion
Source: ..\..\docs\UserGuide_ru.chm; DestDir: {app}; Flags: restartreplace uninsrestartdelete ignoreversion

[InstallDelete]
; Remove old nonlocalized shortcut
Type: files; Name: "{group}\User Guide.lnk"
; Old CVSNT installation
Type: files; Name: {app}\ca.pem
Type: files; Name: {app}\cvs.exe
Type: files; Name: {app}\cvs.exe.local
Type: files; Name: {app}\cvslock.exe
Type: files; Name: {app}\cvsagent.exe
Type: files; Name: {app}\WMFree.exe
Type: files; Name: {app}\charset.dll
Type: files; Name: {app}\cvsapi.dll
Type: files; Name: {app}\cvstools.dll
Type: files; Name: {app}\iconv.dll
Type: files; Name: {app}\mdnsclient.dll
Type: files; Name: {app}\plink.dll
Type: files; Name: {app}\SmartLoader.dll
Type: files; Name: {app}\libeay32_vc71.dll
Type: files; Name: {app}\ssleay32_vc71.dll
Type: files; Name: {app}\protocols\ext.dll
Type: files; Name: {app}\protocols\fork.dll
Type: files; Name: {app}\protocols\gserver.dll
Type: files; Name: {app}\protocols\pserver.dll
Type: files; Name: {app}\protocols\server.dll
Type: files; Name: {app}\protocols\sserver.dll
Type: files; Name: {app}\protocols\ssh.dll
Type: files; Name: {app}\protocols\sspi.dll
Type: files; Name: {app}\triggers\info.dll
Type: files; Name: {app}\xdiff\extdiff.dll
Type: files; Name: {app}\xdiff\xmldiff.dll

[Registry]

#include "registry.iss"

; Icon set paths
; The key "Icons" contains one REG_SZ value for each icon set.
; The name of the value is where the icon files are located under {app}\icons. This is also the value stored in
; Prefs/IconSet when that icon set is selected.
; The value of the value is in Unicode and is the name of the icon set as displayed to the user.

Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "TortoiseCVS"; ValueData: "TortoiseCVS"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "Barracuda"; ValueData: "Barracuda"; Components: "icons\barracuda"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "Bertels"; ValueData: "Stefan Bertels"; Components: "icons\bertels"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "Nikolai"; ValueData: "Nikolai Devereaux"; Components: "icons\nikolai"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "Tbf"; ValueData: "Mathias Hasselmann"; Components: "icons\tbf"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "Timo"; ValueData: "Timo Kauppinen"; Components: "icons\timo"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "Cosmin"; ValueData: "Cosmin Smeu"; Components: "icons\cosmin"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "GregSearle"; ValueData: "Greg Searle"; Components: "icons\gregsearle"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "PierreOssman"; ValueData: "Pierre Ossman"; Components: "icons\PierreOssman"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "NG"; ValueData: "Nikolai Giraldo"; Components: "icons\NG"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "XPStyleRounded"; ValueData: _("XP-style rounded"); Components: "icons\XPStyleRounded"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "Classic"; ValueData: _("Classic TortoiseCVS"); Components: "icons\Classic"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "DanAlpha"; ValueData: "Daniel Jackson DanAlpha"; Components: "icons\DanAlpha"
Root: HKLM32; Subkey: Software\TortoiseCVS\Icons; ValueType: string; ValueName: "visli"; ValueData: _("Chinese characters"); Components: "icons\visli"


; Application sound scheme
Root: HKCU; Subkey: AppEvents\EventLabels\TCVS_Error; ValueType: string; ValueName: ; ValueData: _("Error"); Flags: uninsdeletekey
Root: HKCU; Subkey: AppEvents\Schemes\Apps\TortoiseAct; ValueType: string; ValueName: ; ValueData: "TortoiseCVS"; Flags: uninsdeletekey

; Set network compression to none
Root: HKCU32; Subkey: Software\TortoiseCVS\Prefs\Compression Level; Valuetype: dword; ValueName: Compression Level; ValueData: 0; Flags: uninsdeletevalue createvalueifdoesntexist

; Set installation path
Root: HKLM32; Subkey: Software\TortoiseCVS; ValueType: string; ValueName: "RootDir"; ValueData: "{app}\"

; Enable menu icons
Root: HKCU32; Subkey: Software\TortoiseCVS\Prefs\ContextIcons; ValueType: dword; ValueName: _; ValueData: 1; Flags: uninsdeletevalue createvalueifdoesntexist

; Set cvsignored update interval to 100 seconds
Root: HKCU32; Subkey: Software\TortoiseCVS; ValueType: dword; ValueName: "IgnoredList update interval"; ValueData: 100; Flags: uninsdeletevalue createvalueifdoesntexist

; Set external diff and merge tool to "?" 
Root: HKCU32; Subkey: Software\TortoiseCVS\Prefs\External Merge Application; ValueType: string; ValueName: "_"; ValueData: "?"; Flags: uninsdeletevalue createvalueifdoesntexist
Root: HKCU32; Subkey: Software\TortoiseCVS\Prefs\External Diff Application; ValueType: string; ValueName: "_"; ValueData: "?"; Flags: uninsdeletevalue createvalueifdoesntexist

; Set language to setup language
Root: HKCU32; Subkey: Software\TortoiseCVS\Prefs\LanguageIso; ValueType: string; ValueName: "_"; ValueData: "{language}"; Flags: uninsdeletevalue

; Languages
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages; ValueType: dword; ValueName: "en_GB"; ValueData: "1"
; Remove obsolete values from pre 1.10.10 RC 11 installer
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages; ValueType: none; Flags: deletekey
; Note that language names must be in UTF-8, and should be the native language name.
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ar_TN; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\ar_TN"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ar_TN; ValueType: string; ValueName: "Name"; ValueData: "�����"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ca_ES; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\ca_01"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ca_ES; ValueType: string; ValueName: "Name"; ValueData: "Català"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\cs_CZ; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\cs_CZ"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\cs_CZ; ValueType: string; ValueName: "Name"; ValueData: "Český"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\da_DK; ValueType: dword; ValueName: "Enabled"; ValueData: "0"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\da_DK; ValueType: string; ValueName: "Name"; ValueData: "Dansk"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\de_DE; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\de_DE"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\de_DE; ValueType: string; ValueName: "Name"; ValueData: "Deutsch"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\es_ES; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\es_ES"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\es_ES; ValueType: string; ValueName: "Name"; ValueData: "Español"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\fr_FR; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\fr_FR"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\fr_FR; ValueType: string; ValueName: "Name"; ValueData: "Français"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\hu_HU; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\hu_HU"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\hu_HU; ValueType: string; ValueName: "Name"; ValueData: "Magyar"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\it_IT; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\it_IT"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\it_IT; ValueType: string; ValueName: "Name"; ValueData: "Italiano"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ja_JP; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\ja_JP"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ja_JP; ValueType: string; ValueName: "Name"; ValueData: "Japanese"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ka;    ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\ka_GE"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ka;    ValueType: string; ValueName: "Name"; ValueData: "Georgian"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ko_KR; ValueType: dword; ValueName: "Enabled"; ValueData: "0"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ko_KR; ValueType: string; ValueName: "Name"; ValueData: "Korean"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\nb_NO; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\nb_NO"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\nb_NO; ValueType: string; ValueName: "Name"; ValueData: "Norsk (Bokmål)"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\nl_NL; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\nl_NL"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\nl_NL; ValueType: string; ValueName: "Name"; ValueData: "Nederlands"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\pl_PL; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\pl_PL"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\pl_PL; ValueType: string; ValueName: "Name"; ValueData: "Polski"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\pt_BR; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\pt_BR"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\pt_BR; ValueType: string; ValueName: "Name"; ValueData: "Portugu�s"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ro_RO; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\ro_RO"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ro_RO; ValueType: string; ValueName: "Name"; ValueData: "Română"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ru_RU; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\ru_RU"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\ru_RU; ValueType: string; ValueName: "Name"; ValueData: "Ðóññêèé"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\sl_SI; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\sl_SI"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\sl_SI; ValueType: string; ValueName: "Name"; ValueData: "Slovenian"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\tr_TR; ValueType: dword; ValueName: "Enabled"; ValueData: "0"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\tr_TR; ValueType: string; ValueName: "Name"; ValueData: "Türkçe"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\zh_CN; ValueType: dword; ValueName: "Enabled"; ValueData: "0"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\zh_CN; ValueType: string; ValueName: "Name"; ValueData: "中文 (繁髗)"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\zh_TW; ValueType: dword; ValueName: "Enabled"; ValueData: "1"; Components: "language\zh_TW"
Root: HKLM32; Subkey: Software\TortoiseCVS\Languages\zh_TW; ValueType: string; ValueName: "Name"; ValueData: "中文 (簡單)"

Root: HKLM32; Subkey: Software\CVS\PServer; ValueType: string; ValueName: "HaveBoughtSuite"; ValueData: "yes"

; Create here so they can be removed on uninstall
Root: HKCU32; Subkey: Software\TortoiseCVS; Flags: uninsdeletekey
Root: HKLM32; Subkey: Software\TortoiseCVS; Flags: uninsdeletekey

; Vista UAC fixes

Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5d1cb710-1c4b-11d4-bed5-005004b1f42f} {{000214e6-0000-0000-c000-000000000046} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5d1cb710-1c4b-11d4-bed5-005004b1f42f} {{000214e8-0000-0000-c000-000000000046} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5d1cb710-1c4b-11d4-bed5-005004b1f42f} {{0000010b-0000-0000-c000-000000000046} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB710-1C4B-11D4-BED5-005004B1F42F} {{0C6C4200-C589-11D0-999A-00C04FD655E1} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB711-1C4B-11D4-BED5-005004B1F42F} {{0C6C4200-C589-11D0-999A-00C04FD655E1} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB712-1C4B-11D4-BED5-005004B1F42F} {{0C6C4200-C589-11D0-999A-00C04FD655E1} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB713-1C4B-11D4-BED5-005004B1F42F} {{0C6C4200-C589-11D0-999A-00C04FD655E1} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB714-1C4B-11D4-BED5-005004B1F42F} {{0C6C4200-C589-11D0-999A-00C04FD655E1} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB715-1C4B-11D4-BED5-005004B1F42F} {{0C6C4200-C589-11D0-999A-00C04FD655E1} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB716-1C4B-11D4-BED5-005004B1F42F} {{0C6C4200-C589-11D0-999A-00C04FD655E1} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB710-1C4B-11D4-BED5-005004B1F42F} {{000214E4-0000-0000-C000-000000000046} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB710-1C4B-11D4-BED5-005004B1F42F} {{00021500-0000-0000-C000-000000000046} 0x401"; ValueData: 1; MinVersion: 0, 6
Root: HKLM; Subkey: Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Cached; ValueType: dword; ValueName: "{{5D1CB710-1C4B-11D4-BED5-005004B1F42F} {{000214E9-0000-0000-C000-000000000046} 0x401"; ValueData: 1; MinVersion: 0, 6

[Icons]
Name: {group}\_("Help"); Filename: {app}\Help.html; WorkingDir: {app}; IconIndex: 0
Name: {group}\_("User Guide (English)"); Filename: {app}\UserGuide_en.chm; WorkingDir: {app}; IconIndex: 0
Name: {group}\_("User Guide (French)"); Filename: {app}\UserGuide_fr.chm; WorkingDir: {app}; IconIndex: 0; Components: "language\fr_FR"
Name: {group}\_("User Guide (Russian)"); Filename: {app}\UserGuide_ru.chm; WorkingDir: {app}; IconIndex: 0; Components: "language\ru_RU"
Name: {group}\_("User Guide (Simplified Chinese)"); Filename: {app}\UserGuide_cn.chm; WorkingDir: {app}; IconIndex: 0; Components: "language\zh_CN"
Name: {group}\_("Preferences"); Filename: {app}\TortoiseAct.exe; Parameters: CVSPrefs; IconIndex: 0
Name: {group}\_("About"); Filename: {app}\TortoiseAct.exe; Parameters: CVSAbout; IconIndex: 0

[Run]
; VC9 redistributables
Filename: msiexec.exe; Parameters: "/i ""{tmp}\VC9RunTime.msi"" /passive /norestart"; WorkingDir: {tmp}; StatusMsg: Installing Visual Studio 32-bit runtime...; Flags: runhidden
Filename: msiexec.exe; Parameters: "/i ""{tmp}\VC9RunTimeX64.msi"" /passive /norestart"; WorkingDir: {tmp}; StatusMsg: Installing Visual Studio 64-bit runtime...; Flags: runhidden; Check: IsWin64

; CVSNT installer
Filename: msiexec.exe; Parameters: "/i ""{tmp}\{#CVSNTINSTALLER}"" /passive /norestart"; WorkingDir: {tmp}; StatusMsg: Installing CVSNT...; Flags: runhidden

; TortoiseOverlays
Filename: msiexec.exe; Parameters: "/i ""{tmp}\TortoiseOverlays-1.0.9.17375-win32.msi"" /passive /norestart"; WorkingDir: {tmp}; StatusMsg: Installing 32-bit overlay icons...; Flags: runhidden
Filename: msiexec.exe; Parameters: "/i ""{tmp}\TortoiseOverlays-1.0.9.17375-x64.msi"" /passive /norestart"; WorkingDir: {tmp}; StatusMsg: Installing 64-bit overlay icons...; Flags: runhidden; Check: IsWin64

Filename: {app}\PostInst.exe; WorkingDir: {app}; StatusMsg: Migrating CVS data...; Flags: runhidden

[InstallDelete]
Type: Files; Name: {app}\TortoisePlinkSSH2.bat
Type: Files; Name: {app}\TortoisePlinkWithPassword.bat
Type: Files; Name: {app}\TortoisePlinkWithPort.bat
Type: Files; Name: {app}\TrtseShl.dll

[UninstallDelete]
Type: Files; Name: {app}\TortoiseSetupHelper.exe
Type: Files; Name: {app}\TortoiseSetupHelper64.exe

[Messages]
ConfirmUninstall=_("Are you sure you want to completely remove %1 and all of its components?%n%nPLEASE NOTE that uninstalling requires a reboot. If you want to install another version of TortoiseCVS, there is no need to uninstall first.")

[CustomMessages]
PreparingInstallation=_("Preparing for installation")
InstallerCanDoThis=_("As part of the installation, Windows Explorer needs to be restarted.%nYou have three options:%n- The installer can restart Windows Explorer for you.%n- The installer can restart Windows for you.%n- You can restart Windows manually after the installation.")
RestartExplorer=_("Restart Windows Explorer when necessary")
RestartWindows=_("Restart Windows after installation, or restart Windows manually")
FinishingInstallation=_("Finishing the installation")
RestartingExplorer=_("Restarting Windows Explorer")

[Code]
const CVSNTINSTALLER = '{#CVSNTINSTALLER}';
const OVERLAYS32INSTALLER = '{#OVERLAYS32INSTALLER}';
const OVERLAYS64INSTALLER = '{#OVERLAYS64INSTALLER}';

#include "installer.pas"
