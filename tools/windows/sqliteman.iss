; SPDX-FileCopyrightText: 2026 Petr Vanek
; SPDX-License-Identifier: GPL-3.0-or-later
;
; Inno Setup script for the folder build.ps1 assembles; build.ps1 runs it as
;   iscc /DVersion=x.y.z /DStageDir=<folder> /DSourceDir=<checkout> sqliteman.iss

#ifndef Version
  #error Pass /DVersion=x.y.z
#endif

[Setup]
; Never change: Windows tells this program's installs apart by it.
AppId={{970ED3C9-7B0F-4AA9-8A22-F61062E0CD6E}
AppName=Sqliteman
AppVersion={#Version}
AppPublisher=Petr Vanek
AppPublisherURL=https://github.com/pvanek/sqliteman
DefaultDirName={autopf}\Sqliteman
DefaultGroupName=Sqliteman
DisableProgramGroupPage=yes
LicenseFile={#StageDir}\LICENSE.txt
OutputBaseFilename=Sqliteman-{#Version}-setup
SetupIconFile={#SourceDir}\src\app\icons\sqliteman.ico
UninstallDisplayIcon={app}\bin\sqliteman.exe
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Per user without asking for administrator rights, or for everyone.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Sqliteman"; Filename: "{app}\bin\sqliteman.exe"
Name: "{autodesktop}\Sqliteman"; Filename: "{app}\bin\sqliteman.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\bin\sqliteman.exe"; Description: "{cm:LaunchProgram,Sqliteman}"; Flags: nowait postinstall skipifsilent
