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
; Installs for the user, without asking: /ALLUSERS on the command line (an
; administrator, a company) installs for everybody.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
SetupIconFile=..\..\gui\qt\data\icons\pragma-chess.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
WizardStyle=modern
; Few questions: the language is the system's (asked only when it is none of
; ours), the folder only on the first install, no license to accept (it is
; MIT, in the program's folder), no summary to confirm. Welcome, the options,
; the installation, the end.
ShowLanguageDialog=auto
DisableWelcomePage=no
DisableDirPage=auto
DisableReadyPage=yes
WizardSizePercent=110
; The shoulder of every page, the welcome window's (packaging/assets/make-installer-art.py, [Code]).
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

[Messages]
english.WelcomeLabel1=Welcome to Pragma Chess
english.WelcomeLabel2=Pragma Chess keeps your games in databases and your work in projects, and explains the positions you study.%n%nThis will install it on your computer.
english.FinishedHeadingLabel=Pragma Chess is ready
english.FinishedLabel=Pragma Chess is installed on your computer: you find it in the Start menu.
italian.WelcomeLabel1=Ti diamo il benvenuto in Pragma Chess
italian.WelcomeLabel2=Pragma Chess conserva le tue partite nei database e il tuo lavoro nei progetti, e ti spiega le posizioni che studi.%n%nOra lo installiamo sul computer.
italian.FinishedHeadingLabel=Pragma Chess è pronto
italian.FinishedLabel=Pragma Chess è installato sul computer: lo trovi nel menu Start.
french.WelcomeLabel1=Bienvenue dans Pragma Chess
french.WelcomeLabel2=Pragma Chess garde vos parties dans des bases et votre travail dans des projets, et vous explique les positions que vous étudiez.%n%nIl va maintenant être installé sur votre ordinateur.
french.FinishedHeadingLabel=Pragma Chess est prêt
french.FinishedLabel=Pragma Chess est installé sur votre ordinateur : vous le trouverez dans le menu Démarrer.
german.WelcomeLabel1=Willkommen bei Pragma Chess
german.WelcomeLabel2=Pragma Chess bewahrt Ihre Partien in Datenbanken und Ihre Arbeit in Projekten auf und erklärt Ihnen die Stellungen, die Sie studieren.%n%nEs wird jetzt auf Ihrem Computer installiert.
german.FinishedHeadingLabel=Pragma Chess ist bereit
german.FinishedLabel=Pragma Chess ist auf Ihrem Computer installiert: Sie finden es im Startmenü.
spanish.WelcomeLabel1=Te damos la bienvenida a Pragma Chess
spanish.WelcomeLabel2=Pragma Chess guarda tus partidas en bases de datos y tu trabajo en proyectos, y te explica las posiciones que estudias.%n%nAhora se instalará en tu equipo.
spanish.FinishedHeadingLabel=Pragma Chess está listo
spanish.FinishedLabel=Pragma Chess está instalado en tu equipo: lo encontrarás en el menú Inicio.
portuguese.WelcomeLabel1=Bem-vindo ao Pragma Chess
portuguese.WelcomeLabel2=O Pragma Chess guarda as suas partidas em bancos de dados e o seu trabalho em projetos, e explica as posições que você estuda.%n%nAgora ele será instalado no seu computador.
portuguese.FinishedHeadingLabel=O Pragma Chess está pronto
portuguese.FinishedLabel=O Pragma Chess está instalado no seu computador: você o encontra no menu Iniciar.

[CustomMessages]
english.FilesGroup=Files of Pragma Chess:
italian.FilesGroup=File di Pragma Chess:
french.FilesGroup=Fichiers de Pragma Chess :
german.FilesGroup=Dateien von Pragma Chess:
spanish.FilesGroup=Archivos de Pragma Chess:
portuguese.FilesGroup=Arquivos do Pragma Chess:
english.AssociateProjects=Open Pragma Chess &projects (.pch) and databases (.pdb) with {#AppName}
italian.AssociateProjects=Apri i &progetti (.pch) e i database (.pdb) di Pragma Chess con {#AppName}
french.AssociateProjects=Ouvrir les &projets (.pch) et les bases (.pdb) Pragma Chess avec {#AppName}
german.AssociateProjects=Pragma Chess-&Projekte (.pch) und -Datenbanken (.pdb) mit {#AppName} öffnen
spanish.AssociateProjects=Abrir los &proyectos (.pch) y las bases de datos (.pdb) de Pragma Chess con {#AppName}
portuguese.AssociateProjects=Abrir os &projetos (.pch) e os bancos de dados (.pdb) do Pragma Chess com o {#AppName}
english.ProjectFile=Pragma Chess Project
italian.ProjectFile=Progetto Pragma Chess
french.ProjectFile=Projet Pragma Chess
german.ProjectFile=Pragma Chess-Projekt
spanish.ProjectFile=Proyecto de Pragma Chess
portuguese.ProjectFile=Projeto do Pragma Chess
english.DatabaseFile=Pragma Chess Database
italian.DatabaseFile=Database di Pragma Chess
french.DatabaseFile=Base Pragma Chess
german.DatabaseFile=Pragma Chess-Datenbank
spanish.DatabaseFile=Base de datos de Pragma Chess
portuguese.DatabaseFile=Banco de dados do Pragma Chess

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "associate"; Description: "{cm:AssociateProjects}"; GroupDescription: "{cm:FilesGroup}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; The license is not to be accepted (MIT): it stays with the program.
Source: "..\..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion

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
; Databases: offered, not made the default — .pdb is also Visual Studio's (the application makes it the default only where there is none).
Root: HKA; Subkey: "Software\Classes\.pdb\OpenWithProgids"; ValueType: string; ValueName: "PragmaChess.Database"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\PragmaChess.Database"; ValueType: string; ValueName: ""; ValueData: "{cm:DatabaseFile}"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\PragmaChess.Database\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#AppExe},0"; Tasks: associate
Root: HKA; Subkey: "Software\Classes\PragmaChess.Database\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"" ""%1"""; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExe}\SupportedTypes"; ValueType: string; ValueName: ".pdb"; ValueData: ""; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[Code]
{ The shoulder: the picture on the left of the welcome window of Pragma Chess
  (the wizard image, packaging/assets/make-installer-art.py), on every page —
  not only on the first and last, as Inno Setup draws it. The window grows by
  its width and everything else moves right of it; the first and last pages'
  own picture goes, their texts taking its place, and so does the small
  logo of the inner pages' header, a duplicate. }
var
  Shoulder: TBitmapImage;

{ Moves a control's left edge to Left, its right edge where it was. }
procedure SetLeft(Control: TControl; Left: Integer);
begin
  Control.Width := Control.Width + Control.Left - Left;
  Control.Left := Left;
end;

procedure InitializeWizard;
var
  Side: Integer;
  Margin: Integer;
begin
  Side := WizardForm.WizardBitmapImage.Width;
  WizardForm.ClientWidth := WizardForm.ClientWidth + Side;
  WizardForm.OuterNotebook.Left := WizardForm.OuterNotebook.Left + Side;
  WizardForm.OuterNotebook.Width := WizardForm.OuterNotebook.Width - Side;
  WizardForm.Bevel.Left := WizardForm.Bevel.Left + Side;
  WizardForm.Bevel.Width := WizardForm.Bevel.Width - Side;

  Shoulder := TBitmapImage.Create(WizardForm);
  Shoulder.Parent := WizardForm;
  Shoulder.SetBounds(0, 0, Side, WizardForm.ClientHeight);
  Shoulder.Anchors := [akLeft, akTop, akBottom];
  Shoulder.Stretch := True;
  Shoulder.Bitmap := WizardForm.WizardBitmapImage.Bitmap;

  { The first and last pages: the shoulder is their picture now (their
    texts move to the margin below). }
  WizardForm.WizardBitmapImage.Visible := False;
  WizardForm.WizardBitmapImage2.Visible := False;

  { The inner pages' header: no small logo beside the shoulder. }
  WizardForm.WizardSmallBitmapImage.Visible := False;
  WizardForm.PageNameLabel.Width := WizardForm.PageNameLabel.Width + WizardForm.WizardSmallBitmapImage.Width;
  WizardForm.PageDescriptionLabel.Width := WizardForm.PageDescriptionLabel.Width + WizardForm.WizardSmallBitmapImage.Width;
  { One margin on every page — the first and last ones' texts, the inner
    pages' title, its description (not indented under it, as the classic
    wizard has it) and their contents —, the title's own: the pages read
    as the shoulder beside them, aligned left. }
  Margin := WizardForm.PageNameLabel.Left;
  SetLeft(WizardForm.PageNameLabel, Margin);
  SetLeft(WizardForm.PageDescriptionLabel, Margin);
  SetLeft(WizardForm.InnerNotebook, Margin);
  SetLeft(WizardForm.WelcomeLabel1, Margin);
  SetLeft(WizardForm.WelcomeLabel2, Margin);
  SetLeft(WizardForm.FinishedHeadingLabel, Margin);
  SetLeft(WizardForm.FinishedLabel, Margin);
  SetLeft(WizardForm.RunList, Margin);
  SetLeft(WizardForm.YesRadio, Margin);
  SetLeft(WizardForm.NoRadio, Margin);
end;
