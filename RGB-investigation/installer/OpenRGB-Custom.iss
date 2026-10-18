#define AppVersion "1.0.20261019.1"
[Setup]
AppId={{8972A52B-C337-48F8-9E74-82CE6AD51A29}
AppName=OpenRGB Custom
AppVersion={#AppVersion}
AppPublisher=valer23 (personal custom build)
DefaultDirName=C:\Users\valerian\Apps\OpenRGB-Custom
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=output
OutputBaseFilename=OpenRGB-Custom-Setup-{#AppVersion}
UninstallDisplayIcon={app}\OpenRGB-Custom.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartApplications=no
SetupLogging=yes
InfoBeforeFile=INSTALL-NOTES.txt

[Files]
Source: "payload\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "Settings\*"
Source: "payload\Settings\plugins\OpenRGBEffectsPlugin.dll"; DestDir: "{app}\Settings\plugins"; Flags: ignoreversion
Source: "payload\Settings\OpenRGB.json"; DestDir: "{app}\Settings"; Flags: onlyifdoesntexist uninsneveruninstall
Source: "payload\Settings\profiles\*.json"; DestDir: "{app}\Settings\profiles"; Flags: onlyifdoesntexist uninsneveruninstall

[Icons]
Name: "{userprograms}\OpenRGB Custom"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\Start-OpenRGB-Custom.ps1"" -ShowWindow"; WorkingDir: "{app}"; IconFilename: "{app}\OpenRGB-Custom.exe"

[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\Configure-Startup.ps1"" -Remove"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveCustomStartup"

[Code]
function IsAppRunning(): Boolean;
var Locator, Service, Processes: Variant;
begin
  Locator := CreateOleObject('WbemScripting.SWbemLocator');
  Service := Locator.ConnectServer('', 'root\CIMV2');
  Processes := Service.ExecQuery('SELECT ProcessId FROM Win32_Process WHERE Name=''OpenRGB-Custom.exe''');
  Result := Processes.Count > 0;
end;

function InitializeSetup(): Boolean;
begin
  Result := CompareText(GetEnv('USERPROFILE'), 'C:\Users\valerian') = 0;
  if not Result then MsgBox('This personal installer must run under the valerian Windows account.', mbError, MB_OK);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if IsAppRunning() then
    Result := 'Please exit OpenRGB Custom from its tray menu, then retry. Keep Fan Control running.';
end;

function InitializeUninstall(): Boolean;
begin
  Result := not IsAppRunning();
  if not Result then MsgBox('Please exit OpenRGB Custom from its tray menu before uninstalling. Your profiles will be kept.', mbInformation, MB_OK);
end;

procedure CurStepChanged(CurStep: TSetupStep);
var ResultCode: Integer;
begin
  if CurStep = ssPostInstall then begin
    if not Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
      '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + ExpandConstant('{app}\Configure-Startup.ps1') + '"',
      ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      RaiseException('Could not configure startup. See the installation log.');
    if ResultCode <> 0 then RaiseException('Startup configuration failed. Exit code: ' + IntToStr(ResultCode));
  end;
end;
