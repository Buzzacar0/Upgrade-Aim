# Envio da entrada: pré-visualização, controle virtual, drivers e riscos

## Modos

| Modo | O que acontece | Precisa de | Padrão |
| --- | --- | --- | --- |
| **Pré-visualização segura** | O estado processado é calculado, medido e mostrado, mas não é enviado a lugar nenhum. Nenhum outro programa consegue vê-lo. | nada | **sim** |
| **Pass-through** | O estado original do controle é enviado sem alteração (na pré-visualização, nada sai do processo). Use como botão de pânico e nas comparações A/B. | — | desligado |
| **Controle Xbox 360 virtual** | Um controle virtual ViGEm é criado e recebe o estado processado. A vibração que o jogo manda para o controle virtual é repassada ao controle físico. | driver ViGEmBus; normalmente também o HidHide | desligado; precisa ser ativado em Configurações, depois de um aviso de risco |

## Risco para a conta e para os termos de uso

- Muitos jogos online proíbem softwares ou hardwares não autorizados que modificam a entrada do controle. A política de segurança e punições da Activision cobre isso no Call of Duty. O anti-cheat RICOCHET mira dispositivos que modificam a entrada (Cronus Zen, XIM Matrix) e em fevereiro de 2026 anunciou detecções baseadas no tempo, na consistência e nos padrões de resposta da entrada, e não em assinaturas de dispositivo.
- Um controle virtual alimentado por esta ferramenta é uma camada de modificação da entrada da mesma categoria. Usá-lo no online pode levar a punições na sua conta. O Upgrade Aim não tenta se esconder, escapar ou interferir com anti-cheat, e não vai tentar.
- Os recursos que importam para a mira (deadzone, curva, sensibilidade por eixo, resposta no ADS) têm equivalentes nas configurações de controle da maioria dos jogos e no software oficial do fabricante do controle. O caminho mais seguro é achar os valores aqui (medidos no seu controle) e colocá-los lá.

## Drivers necessários

O Upgrade Aim nunca instala drivers, nunca altera configurações de segurança e roda sem administrador. Os instaladores de drivers precisam de administrador e a decisão de instalar é do usuário.

### ViGEmBus

- Necessário para: só a saída para controle virtual.
- Situação: descontinuado pelo autor (arquivado em nov/2023), sem novas atualizações. É um driver de barramento em modo kernel. Instale só se aceitar rodar um driver de kernel sem manutenção.
- Instalação: baixe a última versão na página oficial de releases do autor no GitHub (`nefarius/ViGEmBus`) e rode o instalador como administrador. Não use instaladores hospedados em outros lugares.
- Conferência: Compatibilidade → Drivers mostra `ViGEmBus: rodando`. `upgradeaim-cli devices` mostra o mesmo.
- Desinstalação: Configurações do Windows → Aplicativos → "Nefarius Virtual Gamepad Emulation Bus Driver".
- **Não verificado durante o desenvolvimento**: o driver não estava instalado no PC de desenvolvimento, então a saída ViGEm foi compilada, mas nunca rodou.

### HidHide (opcional, recomendado com a saída virtual)

- Por quê: com um controle virtual ativo, o jogo normalmente vê o controle físico e o virtual e lê os dois (entrada dupla). O HidHide esconde o dispositivo físico de todos os programas, menos dos que estão na lista de permitidos.
- Configuração: instale o HidHide (administrador), abra o cliente de configuração, adicione `UpgradeAim.exe` (e `upgradeaim-cli.exe`, se usar) à lista de aplicativos permitidos e depois esconda as entradas do Tarantula.
- Atenção: se o Upgrade Aim não estiver na lista de permitidos, ele deixa de conseguir ler o controle. Se você esconder o dispositivo e depois desinstalar o Upgrade Aim, mostre o dispositivo de novo no HidHide antes.
- Não verificado durante o desenvolvimento.

## Comportamento de segurança da saída

| Situação | Comportamento |
| --- | --- |
| Motor iniciando com saída virtual | recusa se o controle virtual for ficar no mesmo slot XInput da fonte de entrada (loop de realimentação) |
| Controle físico desconecta | estado neutro enviado na hora (analógicos no centro, gatilhos e botões soltos). Nova tentativa de conexão a cada 250 ms. |
| Ctrl+Alt+X | controle virtual removido na hora, volta para a pré-visualização |
| Ctrl+Alt+P / segurar Back+Start 1,5 s | liga/desliga o pass-through |
| App fecha normalmente | `Engine::stop()` envia o estado neutro e remove o controle virtual |
| O processo trava | esperado: o Windows fecha o handle do barramento ViGEm e o barramento desconecta os controles virtuais dele. **Não verificado** (sem ViGEmBus no PC de desenvolvimento). |
| Desligar "Permitir saída para controle virtual" | volta para a pré-visualização na hora |

## Usando a saída virtual pela CLI

A CLI se recusa a enviar entrada se você não pedir explicitamente:

```bash
upgradeaim-cli run --profile profiles/tarantula8k_precisao.json --output vigem --confirm-injection
```

Sem `--confirm-injection`, ela mostra o aviso de risco e fecha.
