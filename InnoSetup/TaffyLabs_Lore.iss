; ------------------------------------------------------------------------------
; TaffyLabs OBS Plugin Installer
; Basic Inno Setup script for installing an OBS plugin into the default
; OBS Studio install location on Windows.
;
; Expected staged files (relative to this .iss file):
;   ..\_obs_stage\obs-plugins\64bit\taffylabs_mvp_obs.dll
;   ..\_obs_stage\data\obs-plugins\taffylabs_mvp_obs\...
;
; Build output:
;   InnoSetup\Output\TaffyLabs_OBS_Plugin_Setup.exe
; ------------------------------------------------------------------------------

#define MyAppName "TaffyLabs OBS Plugin"
#ifndef MyAppVersion
#define MyAppVersion "2026.03.23.1500"
#endif
#define MyAppPublisher "TaffyLabs"
#define MyAppURL "https://your-site-here.example"
#define MyPluginDll "taffylabs_mvp_obs.dll"
#define MyPluginFolder "taffylabs_mvp_obs"
#define MyOutputExeName "TaffyLabs_OBS_Plugin_Setup"

[Setup]
; Replace this GUID with your own once and keep it stable forever
AppId={{D6E2D7F1-6D64-4E2C-A7B1-111111111111}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; Default OBS install location
DefaultDirName={autopf}\obs-studio
DefaultGroupName={#MyAppName}

; Output
OutputDir=Output
OutputBaseFilename={#MyOutputExeName}_v{#MyAppVersion}

; Installer style
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64

; Optional boilerplate files
; Create these later if desired
;LicenseFile=license.txt
;InfoBeforeFile=before_install.txt
;InfoAfterFile=after_install.txt

; Uninstaller metadata
UninstallDisplayIcon={app}\bin\64bit\obs64.exe

; Nice defaults
DisableProgramGroupPage=yes
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Plugin DLL
Source: "..\_obs_stage\obs-plugins\64bit\{#MyPluginDll}"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion

; Plugin data folder
Source: "..\_obs_stage\data\obs-plugins\{#MyPluginFolder}\*"; DestDir: "{app}\data\obs-plugins\{#MyPluginFolder}"; Flags: ignoreversion recursesubdirs createallsubdirs

; If you later have dependency DLLs, add them here, for example:
; Source: "..\_obs_stage\obs-plugins\64bit\some_dependency.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion

[Icons]
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]

[Code]
function InitializeSetup(): Boolean;
var
  ObsExe: string;
begin
  ObsExe := ExpandConstant('{autopf}\obs-studio\bin\64bit\obs64.exe');

  if not FileExists(ObsExe) then
  begin
    MsgBox(
      'OBS Studio was not found in the default install location:' + #13#10#13#10 +
      ObsExe + #13#10#13#10 +
      'You can still continue and choose the correct OBS folder manually.',
      mbInformation,
      MB_OK
    );
  end;

  Result := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  ObsExe: string;
begin
  Result := True;

  if CurPageID = wpSelectDir then
  begin
    ObsExe := WizardDirValue + '\bin\64bit\obs64.exe';

    if not FileExists(ObsExe) then
    begin
      if MsgBox(
        'The selected folder does not appear to contain OBS Studio:' + #13#10#13#10 +
        ObsExe + #13#10#13#10 +
        'Install anyway?',
        mbConfirmation,
        MB_YESNO
      ) = IDNO then
      begin
        Result := False;
      end;
    end;
  end;
end;