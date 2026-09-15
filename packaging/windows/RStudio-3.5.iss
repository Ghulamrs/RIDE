; Inno Setup script for RIDE 3.5 - three languages (C, C++, Shalimar),
; four targets (x86_64-windows, x86_64-linux, arm64-darwin, tms6747), the
; VM6747 C6000 emulator and the C<->Shalimar converter.
#define MyName "RIDE 3.5"
#define MyVer  "3.5"
#define Stage  "C:\Users\GRA\rstudio-pkg\stage35"

[Setup]
AppId={{7C3A9E10-35D5-4B7A-9E11-2F5D8A6B0C35}
AppName={#MyName}
AppVersion={#MyVer}
AppPublisher=G. R. Akhtar
DefaultDirName={autopf}\{#MyName}
DefaultGroupName={#MyName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\bin\RStudio.exe
OutputDir=C:\Users\GRA\rstudio-pkg
OutputBaseFilename=RIDE-3.5-setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
WizardStyle=modern
LicenseFile={#Stage}\README.md

[Files]
Source: "{#Stage}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\RIDE 3.5"; Filename: "{app}\bin\RStudio.exe"; WorkingDir: "{app}\examples"
Name: "{group}\RIDE 3.5 (console)"; Filename: "{app}\bin\RStudioConsole.exe"; WorkingDir: "{app}\examples"
Name: "{group}\Express Help"; Filename: "{app}\bin\RStudio.exe"; Parameters: """{app}\EXPRESS-HELP.md"""; WorkingDir: "{app}"
Name: "{group}\Manual"; Filename: "{app}\bin\RStudio.exe"; Parameters: """{app}\help\manual\01-part1-model.md"""; WorkingDir: "{app}"
Name: "{group}\Uninstall RIDE 3.5"; Filename: "{uninstallexe}"
Name: "{autodesktop}\RIDE 3.5"; Filename: "{app}\bin\RStudio.exe"; WorkingDir: "{app}\examples"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked
Name: "addtopath"; Description: "Add the bin folder to PATH (cc1i, cxx1i, shci, vm6747, c2s on the command line)"; Flags: unchecked

[Registry]
Root: HKA; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}\bin"; \
    Tasks: addtopath; Check: NeedsAddPath(ExpandConstant('{app}\bin'))

[Run]
Filename: "{app}\bin\RStudio.exe"; Description: "Launch RIDE 3.5"; Flags: nowait postinstall skipifsilent

[Code]
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  { true only if the bin folder is not already on PATH }
  Result := Pos(';' + Uppercase(Param) + ';', ';' + Uppercase(OrigPath) + ';') = 0;
end;
