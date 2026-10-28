# Compatibilidade

Legenda: **Verificado** = observado em hardware real durante o desenvolvimento, com a evidência listada. **Não verificado** = implementado ou plausível, mas não observado. Nada aqui foi tirado de material de marketing.

## Controles

### GameSir Tarantula 8K

| Item | Situação | Evidência / notas |
| --- | --- | --- |
| Reconhecimento no modo XInput | **Verificado** | `VID_3537 PID_103D`, "Xbox 360 Controller for Windows" (xusb22), slot XInput 0. Interfaces: `IG_00` (gamepad HID, a visão HID do XInput), `MI_01` (consumer control + página de fabricante 0xFFF0). `upgradeaim-cli devices`, 27/10/2026, Windows 11 Pro 26200. |
| Taxa de mudanças de estado XInput | **Verificado** | 7.576 mudanças/s com os analógicos parados (5 s, leitura spin), intervalo médio 132,0 µs, p50 124,5 µs |
| Taxa de relatórios HID (Raw Input, IG_00) | **Verificado** | 7.576 relatórios/s na mesma medição, média 132,0 µs, p99 277,5 µs |
| Ruído em repouso | **Verificado** | cerca de ±20 unidades brutas em todo relatório (DP ≈ 0,0002 normalizado). É por isso que o contador de pacotes avança mesmo parado. |
| Desvios em repouso (unidade de teste) | **Verificado nessa unidade** | analógico direito (+0,0140, +0,0062), esquerdo (+0,0060, +0,0296). Meça a sua unidade. |
| Processamento ao vivo no app (pré-visualização) | **Verificado** | Painel, Lab. do analógico, Lab. de latência e Testes com o analógico movido à mão; o motor viu ≈ 7,4 mil estados novos/s com leitura híbrida a 125 µs |
| Cada analógico na sua visualização | **Verificado** | com os dois analógicos em movimento, o esquerdo marcou (0,209, 0,117) e o direito (−0,437, 0,090), cada um no seu círculo |
| Tipo de conexão (cabo x receptor 2.4 GHz) | Não verificado | os dois aparecem como USB, então a conexão usada nos testes não foi identificada |
| Outros modos (Switch, DirectInput/Android, Bluetooth) | Não verificado | os Product IDs deles não foram observados, então não estão em `devices/` |
| Fonte Raw Input (mapeamento HID) para este controle | Não verificado | a leitura usa o mapeamento genérico XInput-HID (`hid_mapping.verified = false`) |
| Repasse de vibração pelo controle virtual | Não verificado | precisa do ViGEmBus |
| Interação com as configurações do firmware/app da GameSir | Não verificado | deadzones ou curvas do firmware se somam ao processamento do Upgrade Aim |

### Outros controles

- **Qualquer controle XInput** funciona com a fonte XInput sem configuração. Não testado aqui.
- **Controles HID / DirectInput** funcionam com a fonte Raw Input e um arquivo de dispositivo com mapeamento. Não testado aqui. Veja [ADDING_CONTROLLERS.md](ADDING_CONTROLLERS.md).

## Launchers

O núcleo de processamento é o mesmo em todos os launchers. A única diferença é qual controle o jogo acaba lendo. O Upgrade Aim detecta os launchers com as verificações abaixo. **Nenhum launcher ou jogo foi testado com a saída do Upgrade Aim durante o desenvolvimento**, e o checklist do app vem com todos os itens em "Não testado".

| Launcher | Detecção (como) | No PC de desenvolvimento | Testado com um jogo |
| --- | --- | --- | --- |
| Steam | `HKCU\Software\Valve\Steam\SteamPath`, alternativa `HKLM\…\WOW6432Node\Valve\Steam`; processo `steam.exe` | instalado, rodando | não |
| Battle.net | `Uninstall\Battle.net` ou `HKLM\…\Blizzard Entertainment\Battle.net`; processos `Battle.net.exe`, `Agent.exe` | não instalado | não |
| App Xbox / Microsoft Store | pacote `Microsoft.GamingApp_*` registrado para o usuário; processo `XboxPcApp.exe` | não instalado | não |

### Pontos de interação conhecidos (para testar, não são resultados)

**Steam**
- O Steam Input é configurado por jogo (Jogo → Propriedades → Controle). Quando ligado, o Steam lê o controle físico e apresenta ao jogo um controle virtual próprio. As deadzones e curvas dele se somam às do Upgrade Aim.
- O Steam Input também pode capturar o controle virtual do Upgrade Aim. Teste com o Steam Input ligado e desligado para o jogo.

**Battle.net**
- O Battle.net não remapeia controles. O jogo lê os controles diretamente. O principal risco é a entrada dupla (controle físico e virtual visíveis ao mesmo tempo).

**App Xbox / Microsoft Store**
- As versões da Store e do Game Pass leem os controles por XInput/GameInput. Confira se o controle virtual aparece para o jogo, e não só para o app Xbox.

### Procedimento de teste por launcher

Faça primeiro num modo offline ou numa partida privada. Anote cada resultado em **Compatibilidade → Checklist manual** e depois exporte o relatório.

1. **Só o controle físico** (Upgrade Aim em pré-visualização): o jogo responde? No Steam, teste com o Steam Input desligado e ligado.
2. **Controle virtual ativo, físico não escondido**: o jogo responde ao virtual, ao físico ou aos dois? Mexer só o analógico direito alternando entre pass-through e processado mostra qual controle o jogo está usando.
3. **Controle virtual ativo, físico escondido** (HidHide com o Upgrade Aim na lista de permitidos): a entrada dupla sumiu?
4. **Vibração**: as vibrações do jogo chegam ao controle físico?
5. **Desconexão**: desconecte o controle físico no meio do jogo. O controle virtual precisa centralizar na hora (o motor envia um estado neutro), e reconectar precisa voltar a funcionar.
6. Anote junto com cada resultado a versão do launcher, a versão do jogo, a versão do Windows e o perfil do Upgrade Aim.
