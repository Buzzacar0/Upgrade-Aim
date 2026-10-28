# Dependências

Conferido em 27/10/2026. "Situação" é o que foi verificado nessa data, não uma garantia.

| Dependência | Versão | Usada para | Licença | Situação | Como é obtida |
| --- | --- | --- | --- | --- | --- |
| nlohmann/json | 3.11.3 | perfis, configurações, arquivos de dispositivo, relatórios | MIT | mantida ativamente | `FetchContent` do CMake, tarball de release com SHA-256 fixo |
| GoogleTest | 1.15.2 | testes unitários | BSD-3-Clause | mantida ativamente | `FetchContent` do CMake, SHA-256 fixo |
| Qt | 6.8.3 (linha LTS) | interface de desktop (só Widgets) | LGPL-3.0 / comercial | a 6.8 é uma série de suporte de longo prazo | instalada à parte (`aqtinstall` ou o instalador do Qt) |
| ViGEmClient | último commit do autor, `b66d02d` (08/09/2023) | saída opcional para Xbox 360 virtual | MIT | **descontinuada**: o repositório foi arquivado pelo autor em nov/2023 e não recebe correções | `FetchContent` do CMake a partir do GitHub, commit fixo. Compilada como biblioteca estática. |
| ViGEmBus (driver, em tempo de execução) | não incluído | necessário só para a saída virtual | BSD-3-Clause | **descontinuado** junto com o ViGEmClient. Segundo os usuários, a última versão ainda instala e funciona no Windows 10/11, mas isso **não foi verificado aqui**. | instalado pelo usuário (nunca pelo Upgrade Aim) |
| HidHide (driver, em tempo de execução, opcional) | não incluído | esconder o controle físico dos jogos | MIT | mesmo autor do ViGEm; confira a situação antes de usar | instalado pelo usuário (nunca pelo Upgrade Aim) |
| APIs do Windows SDK | 10.0.26100 | XInput 1.4, Raw Input, HID (`hid.lib`), SetupAPI, MMCSS (`avrt`), timers, DWM (barra de título escura) | — | parte do Windows | Windows SDK |

## Notas sobre a pilha de controle virtual

- O autor original mencionou um projeto sucessor. Na data da verificação não havia um substituto público, documentado e com API estável, então o Upgrade Aim mantém o ViGEm atrás de uma interface (`IOutputSink`). Um backend futuro só precisa de uma nova saída.
- Como o cliente e o barramento estão descontinuados, a saída virtual é opcional na compilação (`STICKYAIM_WITH_VIGEM`) e fica desligada até o usuário ativá-la em Configurações.

## Não usadas, e por quê

- **Qt Charts**: widgets `QPainter` próprios são mais leves para dados ao vivo em alta taxa e evitam um módulo só GPL/comercial.
- **SDL / DirectInput**: XInput mais Raw Input cobrem o hardware alvo, com acesso direto ao contador de pacotes e aos relatórios HID.
- **Drivers de kernel próprios**: fora do escopo. Eles precisariam de assinatura, e instalá-los significaria modificar o sistema.
