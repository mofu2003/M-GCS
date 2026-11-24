; Build with:
; ISCC.exe /DMyAppVersion=1.2.0 /DMySourceDir=<bundle> /DMyOutputDir=<dist>
;             /DMyOutputBaseFilename=M-GCS-1.2.0-windows-x64 M-GCS.iss

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef MySourceDir
  #error MySourceDir must point to the Flutter Windows release bundle
#endif
#ifndef MyOutputDir
  #define MyOutputDir "."
#endif
#ifndef MyOutputBaseFilename
  #define MyOutputBaseFilename "M-GCS-windows-x64"
#endif

[Setup]
AppId={{7B9F25AA-40C8-4D24-8C88-5F6EAA4C61F0}
AppName=M-GCS
AppVersion={#MyAppVersion}
AppPublisher=M-GCS
AppPublisherURL=https://github.com/mofu2003/M-GCS
AppSupportURL=https://github.com/mofu2003/M-GCS/issues
AppUpdatesURL=https://github.com/mofu2003/M-GCS/releases
DefaultDirName={localappdata}\Programs\M-GCS
DefaultGroupName=M-GCS
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#MyOutputDir}
OutputBaseFilename={#MyOutputBaseFilename}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\mgcs_frontend.exe

[Files]
Source: "{#MySourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Icons]
Name: "{autoprograms}\M-GCS"; Filename: "{app}\mgcs_frontend.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\M-GCS"; Filename: "{app}\mgcs_frontend.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\mgcs_frontend.exe"; Description: "Launch M-GCS"; Flags: nowait postinstall skipifsilent
