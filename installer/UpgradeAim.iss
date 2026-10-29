; Upgrade Aim installer (Inno Setup 6). Built by scripts/package.ps1, which
; prepares ..\dist\stage (app + Qt runtime + profiles + licenses) and
; ..\dist\redist (VC++ runtime, ViGEmBus and HidHide installers).
;
; Third-party drivers are never installed silently: their own installers run
; with their normal windows, and only if the user keeps the task checked.

#define AppName "Upgrade Aim"
#define AppVersion "0.1.2"
#define Stage "..\dist\stage"
#define Redist "..\dist\redist"

[Setup]
AppId={{A872ABD4-BFA7-4468-AAEC-00D2CB1D5D49}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Upgrade Aim
DefaultDirName={autopf}\Upgrade Aim
DefaultGroupName=Upgrade Aim
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=UpgradeAim-Setup-{#AppVersion}
SetupIconFile=..\src\app\resources\app.ico
UninstallDisplayIcon={app}\bin\UpgradeAim.exe
InfoBeforeFile=antes_de_instalar.txt
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes

[Languages]
Name: "pt"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
pt.TaskDesktop=Criar atalho na área de trabalho
en.TaskDesktop=Create a desktop shortcut
pt.TaskDrivers=Drivers de terceiros (cada um abre o próprio instalador):
en.TaskDrivers=Third-party drivers (each opens its own installer):
pt.TaskVigem=ViGEmBus 1.22.0 — cria o controle virtual que leva a saída do Upgrade Aim ao jogo
en.TaskVigem=ViGEmBus 1.22.0 — creates the virtual controller that carries Upgrade Aim's output to the game
pt.TaskHidHide=HidHide 1.5.230 — esconde o controle físico dos jogos (pede para reiniciar o PC)
en.TaskHidHide=HidHide 1.5.230 — hides the physical controller from games (asks to restart the PC)
pt.RunRuntime=Instalando o Microsoft Visual C++ Runtime...
en.RunRuntime=Installing the Microsoft Visual C++ Runtime...
pt.RunVigem=Abrindo o instalador do ViGEmBus...
en.RunVigem=Opening the ViGEmBus installer...
pt.RunHidHide=Abrindo o instalador do HidHide...
en.RunHidHide=Opening the HidHide installer...
pt.RunApp=Abrir o Upgrade Aim
en.RunApp=Open Upgrade Aim

[Tasks]
Name: "desktopicon"; Description: "{cm:TaskDesktop}"
Name: "vigem"; Description: "{cm:TaskVigem}"; GroupDescription: "{cm:TaskDrivers}"
Name: "hidhide"; Description: "{cm:TaskHidHide}"; GroupDescription: "{cm:TaskDrivers}"

[Files]
Source: "{#Stage}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
Source: "{#Redist}\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
Source: "{#Redist}\ViGEmBus_1.22.0_x64_x86_arm64.exe"; DestDir: "{app}\drivers"; Flags: ignoreversion
Source: "{#Redist}\HidHide_1.5.230_x64.exe"; DestDir: "{app}\drivers"; Flags: ignoreversion
; Shortcuts take their icon from this file: a path Windows never cached, so an
; update over an older version cannot keep showing the old icon.
Source: "..\src\app\resources\app.ico"; DestDir: "{app}"; DestName: "UpgradeAim.ico"; Flags: ignoreversion

; Same AppId in every version, so a new installer updates the old install in
; place. Program files from the old version are removed first so nothing stale
; is left behind (settings, profiles and the activation live in %APPDATA%).
[InstallDelete]
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\share"

[Icons]
Name: "{autoprograms}\Upgrade Aim"; Filename: "{app}\bin\UpgradeAim.exe"; IconFilename: "{app}\UpgradeAim.ico"
Name: "{autodesktop}\Upgrade Aim"; Filename: "{app}\bin\UpgradeAim.exe"; IconFilename: "{app}\UpgradeAim.ico"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; StatusMsg: "{cm:RunRuntime}"; Flags: waituntilterminated
Filename: "{app}\drivers\ViGEmBus_1.22.0_x64_x86_arm64.exe"; StatusMsg: "{cm:RunVigem}"; Tasks: vigem; Flags: waituntilterminated
Filename: "{app}\drivers\HidHide_1.5.230_x64.exe"; StatusMsg: "{cm:RunHidHide}"; Tasks: hidhide; Flags: waituntilterminated
; Windows caches icons by path: refresh it so an update over an older version
; shows the new icon on the taskbar and in the Start menu.
Filename: "{sys}\ie4uinit.exe"; Parameters: "-show"; Flags: runhidden waituntilterminated skipifdoesntexist
Filename: "{app}\bin\UpgradeAim.exe"; Description: "{cm:RunApp}"; Flags: nowait postinstall skipifsilent
