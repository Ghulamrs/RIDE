; Inno Setup script for RIDE 3.0 - three languages (C, C++, Shalimar),
; four targets (x86_64-windows, x86_64-linux, arm64-darwin, tms6747), the
; VM6747 C6000 emulator and the C<->Shalimar converter.
#define MyName "RIDE 3.0"
#define MyVer  "3.0"
#ifndef Stage
#define Stage "C:\Users\GRA\rstudio-pkg\stage30"
#endif
#ifndef OutDir
#define OutDir "C:\Users\GRA\rstudio-pkg"
#endif

[Setup]
AppId={{6B2D8F04-30C0-4A19-8D22-1E4C7A5B0C30}
AppName={#MyName}
AppVersion={#MyVer}
AppPublisher=G. R. Akhtar
DefaultDirName={autopf}\{#MyName}
DefaultGroupName={#MyName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\bin\RStudio.exe
OutputDir={#OutDir}
OutputBaseFilename=RIDE-3.0-setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
WizardStyle=modern
; Tell Explorer the environment changed, so a new console sees the PATH.
ChangesEnvironment=yes
LicenseFile={#Stage}\README.md

[Files]
Source: "{#Stage}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\RIDE 3.0"; Filename: "{app}\bin\RStudio.exe"; WorkingDir: "{app}\examples"
Name: "{group}\RIDE 3.0 (console)"; Filename: "{app}\bin\RStudioConsole.exe"; WorkingDir: "{app}\examples"
Name: "{group}\Express Help"; Filename: "{app}\EXPRESS-HELP.html"; WorkingDir: "{app}"
Name: "{group}\Manual"; Filename: "{app}\help\manual.html"; WorkingDir: "{app}"
Name: "{group}\User Guide"; Filename: "{app}\help\guide.html"; WorkingDir: "{app}"
Name: "{group}\Uninstall RIDE 3.0"; Filename: "{uninstallexe}"
Name: "{autodesktop}\RIDE 3.0"; Filename: "{app}\bin\RStudio.exe"; WorkingDir: "{app}\examples"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"
Name: "addtopath"; Description: "Add the bin folder to PATH (cc1i, cxx1i, shci, c2s on the command line)"; Flags: unchecked

[Registry]
; The user's own PATH, HKCU\Environment, on purpose. The machine-wide one is
; not "HKLM\Environment" but HKLM\SYSTEM\CurrentControlSet\Control\Session
; Manager\Environment, so with the installer elevated (Program Files) the
; earlier "HKA" spelling wrote a key nothing reads and PATH never changed.
; Every account that installs RIDE already has an HKCU Path, which is where a
; per-user tool belongs anyway; NeedsAddPath reads the same hive.
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}\bin"; \
    Tasks: addtopath; Check: NeedsAddPath(ExpandConstant('{app}\bin'))

[Run]
Filename: "{app}\bin\RStudio.exe"; Description: "Launch RIDE 3.0"; Flags: nowait postinstall skipifsilent

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

{ Take the bin folder back out of the user's PATH on uninstall; Inno restores
  nothing on its own, and leaving a dead entry is what "addtopath" would
  otherwise cost the next install into a different folder. At usUninstall,
  not usPostUninstall: tried on the box, the later step never reached this
  code and PATH kept the entry. The Log lines land in the uninstall log. }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Bin, Path: string;
  P: Integer;
begin
  if CurUninstallStep <> usUninstall then exit;
  Bin := ExpandConstant('{app}\bin');
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', Path) then exit;
  P := Pos(';' + Uppercase(Bin) + ';', ';' + Uppercase(Path) + ';');
  Log('PATH entry ' + Bin + ' found at ' + IntToStr(P) + ' in ' + Path);
  if P = 0 then exit;
  { P counts from the leading ';' we added; the entry starts at P in Path
    when it is first, else the ';' before it is at P-1 }
  if P = 1 then
    Delete(Path, 1, Length(Bin) + 1)   { "bin;" at the front, or all of it }
  else
    Delete(Path, P - 1, Length(Bin) + 1);  { ";bin" }
  if RegWriteExpandStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', Path) then
    Log('PATH is now ' + Path)
  else
    Log('PATH could not be written');
end;
