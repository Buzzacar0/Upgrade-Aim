# Compilação e instalação

## Requisitos

| Componente | Versão usada e verificada | Notas |
| --- | --- | --- |
| Windows | 11 Pro 10.0.26200 (x64) | Precisa do Windows 10 1803 ou mais novo para os timers de alta resolução. Versões mais antigas usam timers comuns. |
| Visual Studio 2022 ou Build Tools 2022 | Build Tools 2022, MSVC x64 | Carga de trabalho "Desenvolvimento para desktop com C++" / `Microsoft.VisualStudio.Workload.VCTools` |
| Windows SDK | 10.0.26100 | instalado junto com a carga de trabalho |
| CMake | 3.24 ou mais novo | o CMake que vem com o VS funciona |
| Ninja | qualquer versão recente | vem com o VS, ou `winget install Ninja-build.Ninja` |
| Qt | 6.8.3, `msvc2022_64` | só necessário para o app de desktop. Qualquer Qt ≥ 6.5 deve funcionar. |
| Git | qualquer | o CMake baixa o ViGEmClient do GitHub |

GoogleTest, nlohmann/json e ViGEmClient são baixados pelo CMake na configuração (versões e hashes fixos, veja [DEPENDENCIES.md](DEPENDENCIES.md)). A primeira configuração precisa de internet.

## Instalando as ferramentas (o que foi feito no PC de desenvolvimento)

Rode num prompt como administrador; cada passo instala software no sistema todo.

```bash
winget install --id Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --norestart --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.26100 --add Microsoft.VisualStudio.Component.VC.CMake.Project"
```

```bash
winget install --id Kitware.CMake
```

```bash
winget install --id Ninja-build.Ninja
```

O Qt foi instalado com o `aqtinstall`, um baixador não oficial (o instalador online oficial do Qt também serve):

```bash
python -m pip install aqtinstall
```

```bash
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir C:\Qt
```

## Compilando

O jeito mais simples é dar dois cliques em `Upgrade Aim.bat`: se o app não estiver compilado, ele compila e depois abre.

O script de compilação acha o Visual Studio, importa o ambiente de desenvolvimento x64, acha o Qt em `C:\Qt\6.*\msvc2022_64` (ou usa `-QtRoot` / `QT_ROOT`), configura, compila e roda os testes.

```bash
powershell -ExecutionPolicy Bypass -File scripts/build.ps1
```

Opções:

| Opção | Efeito |
| --- | --- |
| `-Preset release` | padrão: app + CLI + testes, RelWithDebInfo |
| `-Preset debug` | compilação de depuração |
| `-Preset core` | só o núcleo portável e os testes dele (sem Qt, sem APIs do Windows) |
| `-QtRoot <pasta>` | pasta do kit do Qt |
| `-NoTest` | não roda o ctest |
| `-Deploy` | roda o `windeployqt` para o `UpgradeAim.exe` funcionar fora do ambiente de compilação |

Equivalente manual, num "x64 Native Tools Command Prompt for VS 2022":

```bash
cmake --preset release -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64
```

```bash
cmake --build --preset release
```

```bash
ctest --preset release
```

Opções do CMake: `STICKYAIM_BUILD_APP`, `STICKYAIM_BUILD_CLI`, `STICKYAIM_BUILD_TESTS`, `STICKYAIM_WITH_VIGEM` (compila a saída opcional para controle virtual; padrão ON), `STICKYAIM_CORE_ONLY`. O prefixo `STICKYAIM_` vem do nome de trabalho do projeto.

## Rodando

- App: `build/release/src/app/UpgradeAim.exe`. Ele abre a tela inicial (idioma) e começa em pré-visualização segura com o primeiro controle XInput conectado, ou com o sinal de demonstração se não houver nenhum.
- CLI: `build/release/src/cli/upgradeaim-cli.exe help` (a saída da CLI é em inglês).
- Dados do usuário (configurações, calibração, perfis do usuário, logs): `%APPDATA%\UpgradeAim`. Na primeira execução, se existir a pasta antiga `%APPDATA%\StickyAimLab`, o conteúdo dela é copiado para a nova (a antiga não é apagada).
- `UpgradeAim.exe --lang pt-BR` ou `--lang en` pula a tela inicial.
- `UpgradeAim.exe --screenshots <pasta>` salva cada página em PNG e fecha (usado para as capturas deste repositório). `--launcher-shot <arquivo.png>` faz o mesmo com a tela inicial.

Não é preciso ser administrador para compilar nem para rodar o Upgrade Aim. Só os drivers opcionais de terceiros de [DELIVERY.md](DELIVERY.md) precisam de administrador para instalar.

## Instalando

```bash
cmake --install build/release --prefix "C:/Program Files/UpgradeAim"
```

Isso instala os executáveis em `bin/` e os perfis e arquivos de dispositivo em `share/upgradeaim/`, onde o app os encontra. Rode o `windeployqt` no `UpgradeAim.exe` instalado (ou compile com `-Deploy`).
