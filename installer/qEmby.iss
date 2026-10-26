; ============================================================
; qEmby Inno Setup Script
; ============================================================
; 用法 (命令行编译):
;   iscc /DAppVersion=0.0.2 /DSourceDir=..\dist\qEmby qEmby.iss
;
; 参数说明:
;   AppVersion  - 版本号 (默认 0.0.2)
;   SourceDir   - 打包源目录，即 windeployqt 输出目录 (默认 ..\dist\qEmby)
; ============================================================

#ifndef AppVersion
  #define AppVersion "0.0.2"
#endif

#ifndef SourceDir
  #define SourceDir "..\dist\qEmby"
#endif

#define AppName "qEmby"
#define AppPublisher "AlanHJ"
#define AppURL "https://github.com/AlanHJ/qEmby"
#define AppExeName "qEmbyApp.exe"

[Setup]
AppId={{B8F3D2A1-7C4E-4A9B-8D5F-1E2C3A4B5D6E}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} v{#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
OutputDir=..\release
OutputBaseFilename=qEmby-v{#AppVersion}-Win-x64-Setup
SetupIconFile=..\src\qEmbyApp\resources\qEmbyApp.ico
UninstallDisplayIcon={app}\{#AppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
LZMAUseSeparateProcess=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
WizardImageFile=compiler:WizClassicImage.bmp
WizardSmallImageFile=compiler:WizClassicSmallImage.bmp
PrivilegesRequired=admin
MinVersion=10.0
ShowLanguageDialog=auto

[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode

[Files]
; 主程序和所有 DLL / 资源
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 清理运行时生成的日志/缓存等
Type: filesandordirs; Name: "{app}\logs"
Type: filesandordirs; Name: "{app}\cache"
