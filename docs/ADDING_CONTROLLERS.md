# Como adicionar suporte a outro controle

## 1. Controles XInput: não precisa fazer nada

Se o controle aparece num slot XInput (o `upgradeaim-cli devices` lista ele em "XInput slots"), a fonte XInput já funciona. Um arquivo de dispositivo só adiciona nome, notas e um mapeamento HID.

## 2. Identifique o dispositivo

```bash
upgradeaim-cli devices
```

Anote o `VID_xxxx PID_yyyy` da interface gamepad/joystick, se ela está marcada como `[XInput HID view]` e o nome do produto. Modos diferentes do controle (XInput, DirectInput, Switch, receptor sem fio) normalmente têm PIDs diferentes. Liste só os que você observou.

## 3. Meça o controle

```bash
upgradeaim-cli measure --seconds 10
```

Mexa o analógico sem parar. Anote as taxas e os intervalos do XInput e do HID.

```bash
upgradeaim-cli calibrate --rest 4 --range 6
```

Anote o desvio e o ruído em repouso.

## 4. Escreva um arquivo de dispositivo

Copie `devices/TEMPLATE.json.txt` para `devices/<fabricante>_<modelo>.json`:

```json
{
  "name": "Fabricante Modelo",
  "vendor_id": "1234",
  "modes": [
    { "product_id": "ABCD", "mode": "DirectInput", "connection": "USB",
      "verification": "Observado em 27/10/2026 no Windows 11 26200: <o que você viu, com números>" }
  ],
  "notes": ["Fatos que você mediu."],
  "unverified": ["Afirmações que você não conseguiu medir."],
  "hid_mapping": {
    "preset": "generic",
    "lx": "X", "ly": "-Y", "rx": "Z", "ry": "-Rz", "lt": "Rx", "rt": "Ry",
    "buttons": { "1": "X", "2": "A", "3": "B", "4": "Y", "5": "LB", "6": "RB", "9": "Back", "10": "Start", "11": "LS", "12": "RS" },
    "hat_to_dpad": true,
    "verified": false
  }
}
```

Regras:

- Deixe `verification` vazio se você não observou aquele modo. Deixe `"verified": false` no mapeamento até conferir cada eixo e cada botão no Painel.
- Eixos: `X Y Z Rx Ry Rz Slider Dial`, com `-` opcional na frente para inverter e `+` ou `-` opcional no final para usar só a metade positiva ou negativa de um eixo centrado (gatilhos combinados). `none` desativa o destino.
- Presets: `xinput_hid` (a visão HID que o Windows cria para controles XInput: X/Y, Rx/Ry, Z = gatilhos combinados) ou `generic` (layout DirectInput comum). As chaves explícitas substituem o preset.
- Nomes de botão: `A B X Y LB RB Back Start LS RS Guide DpadUp DpadDown DpadLeft DpadRight`. Os números são os usages de botão HID (começando em 1).

## 5. Confira o mapeamento

1. Abra o app → Painel → Fonte de entrada → escolha a entrada HID do seu dispositivo.
2. Mexa cada analógico e gatilho e aperte cada botão. Confira os círculos do Painel e os gráficos do Lab. do analógico: para cima tem que ser +Y e para a direita +X.
3. Se um eixo estiver invertido ou trocado, corrija a especificação e clique em "Procurar".
4. Quando tudo estiver certo, mude para `"verified": true` e descreva em `verification` o que você conferiu.

## 6. Adicione um teste (opcional, mas recomendado)

`tests/test_config.cpp` → `DeviceDb.ShippedDevicesLoad` mostra como verificar que um arquivo de dispositivo carrega e reconhece o VID/PID.

## 7. Novos tipos de fonte

Para suportar uma nova API (por exemplo GameInput), implemente `sa::IInputSource` (`src/core/include/stickyaim/core/io.hpp`):

- `open()` retorna false enquanto o dispositivo não estiver presente (o motor tenta de novo).
- `poll()` não pode bloquear nem alocar memória. Retorne `NewSample` só para dados novos e preencha `RawSample::sequence` se a API tiver um contador (e então retorne true em `hasDeviceSequence()`, para os pacotes perdidos serem contados).
- Adicione a fonte em `AppController::makeSource()`.

O núcleo de processamento, o motor, a telemetria e todas as páginas funcionam sem mudanças.
