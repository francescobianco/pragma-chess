; Inno Setup script of the Windows installer. Built by build.ps1, which passes
;   /DAppVersion=x.y.z /DSourceDir=<deployed app folder> /DOutputDir=<dist>
; Needs Inno Setup 6.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #error SourceDir must point to the folder prepared by build.ps1
#endif
#ifndef OutputDir
  #define OutputDir "."
#endif

#define AppName "Pragma Chess"
#define AppExe "pragma-chess.exe"
#define AppUrl "https://github.com/francescobianco/pragma-chess"

[Setup]
; Never change the AppId: it is how upgrades find the installed copy.
AppId={{6F1B7C2E-4A8D-4E57-9B0C-3D2A8E5F7C41}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Francesco Bianco
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}/releases
AppCopyright=Copyright (C) Francesco Bianco and contributors
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; Installs for everybody when run as administrator, else only for the user,
; and lets the user choose.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
LicenseFile=..\..\LICENSE
SetupIconFile=..\..\gui\qt\data\icons\pragma-chess.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
WizardStyle=modern
WizardImageFile=wizard-image-100.bmp,wizard-image-150.bmp,wizard-image-200.bmp
WizardSmallImageFile=wizard-small-100.bmp,wizard-small-150.bmp,wizard-small-200.bmp
Compression=lzma2/ultra64
SolidCompression=yes
ChangesAssociations=yes
CloseApplications=yes
RestartApplications=no
OutputDir={#OutputDir}
OutputBaseFilename=PragmaChess-{#AppVersion}-windows-x64-setup

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"

[CustomMessages]
english.AssociateProjects=Open Pragma Chess &projects (.pch) with {#AppName}
italian.AssociateProjects=Apri i &progetti Pragma Chess (.pch) con {#AppName}
french.AssociateProjects=Ouvrir les &projets Pragma Chess (.pch) avec {#AppName}
german.AssociateProjects=Pragma Chess-&Projekte (.pch) mit {#AppName} öffnen
spanish.AssociateProjects=Abrir los &proyectos de Pragma Chess (.pch) con {#AppName}
portuguese.AssociateProjects=Abrir os &projetos do Pragma Chess (.pch) com o {#AppName}
english.ProjectFile=Pragma Chess Project
italian.ProjectFile=Progetto Pragma Chess
french.ProjectFile=Projet Pragma Chess
german.ProjectFile=Pragma Chess-Projekt
spanish.ProjectFile=Proyecto de Pragma Chess
portuguese.ProjectFile=Projeto do Pragma Chess

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "associate"; Description: "{cm:AssociateProjects}"; GroupDescription: "{cm:AssocFileExtension,{#AppName},.pch}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.pch"; ValueType: string; ValueName: ""; ValueData: "PragmaChess.Project"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.pch\OpenWithProgids"; ValueType: string; ValueName: "PragmaChess.Project"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\PragmaChess.Project"; ValueType: string; ValueName: ""; ValueData: "{cm:ProjectFile}"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\PragmaChess.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\PragmaChess.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\SupportedTypes"; ValueType: string; ValueName: ".pch"; ValueData: ""; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent
