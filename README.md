# Upgrade Aim

Um laboratório de entrada de controle para Windows. Com ele você mede o que o controle realmente envia, ajusta a resposta do analógico de mira (deadzones, curvas, faixas fina e rápida, suavização) e compara o sinal original com o processado usando testes objetivos e repetíveis.

Hardware principal: **GameSir Tarantula 8K**, verificado neste hardware no modo XInput (veja [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md)). Qualquer controle XInput funciona; controles HID (estilo DirectInput) funcionam com um arquivo de mapeamento.

![Lab. do analógico](docs/screenshots/2_stick_lab.png)

## O que ele faz

- **Leitura**: XInput (por mudança de estado, com contador de pacotes), monitoramento de relatórios HID por dispositivo via Raw Input e uma fonte de demonstração/reprodução determinística. Todas as amostras recebem um carimbo de tempo monotônico baseado em QPC.
- **Núcleo de processamento** (C++20 portável, sem dependência do sistema operacional): calibração de centro e alcance → suavização opcional (EMA ou One Euro, com bypass de flick) → deadzone radial ou axial com limite externo → curva de resposta (linear, potência, exponencial ou spline monotônica personalizada) com faixas de ajuste fino e de movimento rápido → sensibilidade por eixo → saturação → anti-deadzone opcional. Contextos separados de **hip-fire** e **ADS** (gatilho esquerdo com histerese e transição gradual opcional). Os perfis são compilados em tabelas de consulta, então o processamento não aloca memória e é determinístico.
- **Envio**: **pré-visualização segura** (padrão; nada sai do processo) ou um controle Xbox 360 virtual via ViGEmBus, que você **precisa ativar** de propósito. O núcleo é o mesmo para todos os launchers; eles só mudam qual controle o jogo lê.
- **Diagnóstico**: visualização ao vivo dos analógicos e das curvas, histogramas de intervalo entre amostras, variação e tempo de processamento, contadores de pacotes perdidos e de atrasos do agendador, análise de drift e ruído, detecção de launchers e drivers, e um checklist manual de compatibilidade.
- **Testes**: bateria de sinais sintéticos (ruído em repouso, drift, degraus, rastreio, microajustes, flicks, círculos completos) e capturas ao vivo, repassadas pelo perfil ativo e por uma referência. Os relatórios mantêm as medições objetivas separadas das notas subjetivas.

## O que ele não faz

- Não detecta inimigos, não adquire alvos, não compensa recuo, não tem macros, turbo nem qualquer mira automática. Ele só remodela o sinal que o próprio jogador produz.
- Não altera o aim assist do jogo. Mudar a entrada do analógico não aumenta, por si só, a força do aim assist, e esta ferramenta não afirma isso.
- Não instala drivers, não altera configurações de segurança e não tenta se esconder de anti-cheat.

> **Risco para a conta.** A saída para controle virtual é, na prática, uma camada que modifica a entrada. O anti-cheat RICOCHET do Call of Duty mira dispositivos que modificam a entrada (Cronus Zen, XIM) e em fevereiro de 2026 anunciou detecções baseadas no tempo e nos padrões da entrada. Usar a saída virtual no online pode violar os termos do jogo e colocar sua conta em risco de banimento. O caminho seguro, que é o padrão, é: medir e ajustar aqui e depois colocar valores equivalentes nas configurações de controle do próprio jogo ou no software oficial da GameSir. Veja [docs/DELIVERY.md](docs/DELIVERY.md).

## Começo rápido

Dê dois cliques em **`Upgrade Aim.bat`**, na pasta do projeto. Ele abre o app e, se o app ainda não estiver compilado, compila antes (com `-Deploy`, sem rodar os testes).

Para compilar manualmente (pré-requisitos e instruções completas em [docs/BUILD.md](docs/BUILD.md)):

```bash
powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Deploy
```

```bash
build/release/src/app/UpgradeAim.exe
```

O app abre numa tela inicial onde você escolhe o idioma: **Português (Brasil)** (padrão) ou **English**. A escolha fica salva em `%APPDATA%\UpgradeAim\launcher.ini`. "Trocar imagem de fundo…" define a imagem da tela inicial (mostrada com 50 % de opacidade). Você também pode colocar um `launcher_background.jpg` ou `.png` em `%APPDATA%\UpgradeAim\`. Nenhuma arte de jogo vem junto: use uma imagem que você tenha direito de usar. `--lang pt-BR` ou `--lang en` pula a tela inicial.

As traduções ficam em `src/app/i18n_pt_br.cpp`, indexadas pelo texto em inglês passado para `T()`. As chaves internas dos arquivos de configuração continuam em inglês.

Diagnóstico pela linha de comando (sem interface; a saída da CLI é em inglês):

```bash
build/release/src/cli/upgradeaim-cli.exe devices
```

```bash
build/release/src/cli/upgradeaim-cli.exe measure --seconds 10
```

## Páginas do app

| Página | Para que serve |
| --- | --- |
| Painel | Fonte de entrada, controle detectado, estado do motor, taxa de leitura ao vivo (Hz), tempo de resposta sem e com o app (ms), analógicos ao vivo, troca de perfil |
| Lab. do analógico | Cada analógico com original x processado, gráfico de resposta interativo (pipeline completo ou formato da curva com pontos arrastáveis), gráfico no tempo, calibração em repouso e de alcance |
| Perfis de mira | Contextos hip-fire e ADS: deadzone, curvas, faixas fina e rápida, sensibilidade X/Y, suavização, saturação; limites e transição do ADS; arquivos de perfil |
| Lab. de latência | Histogramas de intervalo entre amostras, tempo de processamento e caminho do software; tempo dos relatórios HID por dispositivo; modo de leitura |
| Compatibilidade | Slots XInput, interfaces HID com o banco de dispositivos, drivers, launchers, checklist manual, exportação em Markdown |
| Testes | Comparações sintéticas e ao vivo de original x processado contra uma referência, exportação em JSON com notas subjetivas separadas |
| Configurações | Ativação da saída virtual (com aviso de risco), combo de pânico, exportar/importar/restaurar, pasta de dados |

Controles de segurança: **Ctrl+Alt+P** liga/desliga o pass-through, **Ctrl+Alt+X** remove o controle virtual e volta para a pré-visualização, **Ctrl+Alt+1…9** troca de perfil, e segurar **Back+Start** por 1,5 s no controle liga/desliga o pass-through. Se o controle desconectar, a saída é centralizada e liberada na hora.

## Medido no PC de desenvolvimento (27/10/2026)

GameSir Tarantula 8K, USB, modo XInput, Windows 11 Pro 26200. Detalhes e números brutos em [docs/BENCHMARKS.md](docs/BENCHMARKS.md).

| Medida | Resultado |
| --- | --- |
| Mudanças de estado XInput, analógicos parados (5 s, leitura spin) | 7.576 /s, intervalo médio 132,0 µs, p50 124,5 µs, p99 258,5 µs |
| Relatórios HID via Raw Input (interface IG_00), mesma medição | 7.576 /s, média 132,0 µs, p99 277,5 µs |
| Variação em repouso | cerca de ±20 unidades brutas (DP ≈ 0,0002 normalizado) em todo relatório |
| Motor, híbrido 125 µs, analógicos em movimento | ≈ 7.400 estados novos/s; 0,2–2 % dos pacotes do controle não observados |
| `Processor::process()` | p50 0,21 µs, p99 1,21 µs por amostra; média do lote ≈ 90 ns |
| Caminho do software (leitura → envio na pré-visualização) | p99 1,5 µs |
| Atraso adicionado pelo app (Painel) | ≈ 0,06 ms em média, pior caso ≈ 0,13 ms |

Esses números cobrem só o caminho do software no PC. A latência de USB, driver, jogo e monitor não está incluída e não foi medida.

## Não verificado

- Saída para controle virtual com um jogo: o controle virtual foi criado e recebeu a entrada processada (28/10/2026), mas ainda não foi testado dentro de um jogo.
- Qualquer jogo, incluindo o MW4, e o comportamento de qualquer launcher (Steam, Battle.net, app Xbox). O checklist de compatibilidade vem vazio.
- Os outros modos do Tarantula 8K (2.4 GHz, Bluetooth, Switch, DirectInput) e o mapeamento HID dele pela fonte Raw Input.

## Estrutura do repositório

```
src/core       núcleo de processamento portável (curvas, deadzones, pipeline, estatísticas, análise, configuração)
src/platform   Windows: XInput, Raw Input HID, saída ViGEm, detecção de launchers e drivers, timers
src/engine     thread de entrada em tempo real (fonte -> processamento -> saída), telemetria, troca de perfil
src/cli        diagnóstico upgradeaim-cli
src/app        app de desktop em Qt 6
tests          testes GoogleTest (núcleo + motor)
profiles       perfis de exemplo para o Tarantula 8K
devices        banco de dispositivos (só observações verificadas)
docs           arquitetura, compilação, dependências, compatibilidade, envio, benchmarks, testes, solução de problemas
```

Os nomes internos do código (namespace `sa`, pastas `include/stickyaim/`, opções `STICKYAIM_*` do CMake) vêm do nome de trabalho do projeto e não aparecem para o usuário.

## Documentação

- [Arquitetura e escolhas técnicas](docs/ARCHITECTURE.md)
- [Compilação e instalação](docs/BUILD.md)
- [Dependências](docs/DEPENDENCIES.md)
- [Compatibilidade de controles e launchers](docs/COMPATIBILITY.md)
- [Envio da entrada, drivers e riscos](docs/DELIVERY.md)
- [Benchmarks](docs/BENCHMARKS.md)
- [Testes e o procedimento de comparação original x processado](docs/TESTING.md)
- [Como adicionar outro controle](docs/ADDING_CONTROLLERS.md)
- [Solução de problemas](docs/TROUBLESHOOTING.md)
- [Distribuição: instalador e códigos de ativação](docs/DISTRIBUTION.md)

Ainda não foi escolhida uma licença para este repositório. Os componentes de terceiros mantêm suas próprias licenças (veja [docs/DEPENDENCIES.md](docs/DEPENDENCIES.md)).
