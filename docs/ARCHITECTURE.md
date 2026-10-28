# Arquitetura

## Camadas

```
            ┌──────────────────────── Interface Qt (src/app) ──────────────────┐
            │ páginas, editor de curva, gráficos ◄── telemetria (fila SPSC, 64k) ┐
            └──────────────┬──────────────────────────────────────────────────┘ │
       publishProfile()    │ (troca de ponteiro)                               │
                           ▼                                                   │
 ┌──────────────┐   ┌───────────────────── Thread do motor (src/engine) ──────┴──┐   ┌──────────────┐
 │ IInputSource │──►│ ritmo → leitura → [combo de pânico] → process → envio     │──►│ IOutputSink  │
 │  XInput      │   │ desconexão → saída neutra → tentativa de reconexão         │   │  Preview     │
 │  HID (Raw In)│   └─────────────────────────────────────────────────────────────┘   │  ViGEm X360  │
 │  Replay/Demo │                                                                       └──────────────┘
 └──────────────┘
                 Núcleo de processamento (src/core): C++20 portável, sem chamadas ao sistema, com testes unitários
```

- **Leitura** (`IInputSource`) só lê e carimba o tempo.
- **Processamento** (`Processor`, `CompiledProfile`) é só cálculo sobre amostras e carimbos de tempo.
- **Envio** (`IOutputSink`) só escreve. A pré-visualização é o padrão e a única saída que não afeta outro programa.

A parte específica de cada launcher nem passa pelo processamento: Steam, Battle.net e o app Xbox leem o controle que o Windows mostrar. Eles são tratados por detecção e documentação (`system_probe.cpp`, [COMPATIBILITY.md](COMPATIBILITY.md)), e não por motores separados.

## Pipeline de processamento (analógico de mira)

| Etapa | Módulo | Notas |
| --- | --- | --- |
| Normalização | `types.cpp` | int16 → [-1, 1] simétrico; −32768 vira −1 |
| Calibração | `calibration.cpp` | subtrai o centro medido e reescala cada meio-eixo até o extremo medido |
| Suavização (opcional) | `smoothing.cpp` | Nenhuma / EMA (exata em dt) / One Euro; bypass de flick acima de uma velocidade do analógico; reinicia em lacunas > 100 ms |
| Deadzone | `deadzone.cpp` | Nenhuma / Radial / Axial; limites interno e externo, reescala linear entre eles (sem perder alcance) |
| Resposta | `response.cpp`, `curve.cpp` | curva + faixas fina e rápida compiladas numa tabela (LUT) de 4097 entradas |
| Sensibilidade | `pipeline.cpp` | ganhos X e Y separados |
| Saturação | `pipeline.cpp` | circular (|v| ≤ 1) ou por eixo |
| Anti-deadzone | `pipeline.cpp` | saída mínima opcional, para anular uma deadzone do jogo que não dá para zerar |

Contextos: hip-fire e ADS têm cada um uma configuração completa. O ADS entra com LT ≥ `adsEnter` e sai com LT ≤ `adsExit`. Uma transição gradual opcional mistura as duas respostas durante `contextTransitionMs`.

O analógico de movimento recebe calibração e deadzone radial. Os gatilhos recebem limites. Os botões passam direto.

### Por que uma LUT com as faixas integradas

As faixas fina e rápida são definidas como **multiplicadores de inclinação** sobre um intervalo de magnitude. A LUT é a integral normalizada de (inclinação da curva × multiplicador da faixa), então:

- ela é monotônica por construção (todos os multiplicadores são positivos);
- a deflexão total continua chegando na saída total (normalização), então uma faixa fina nunca custa velocidade máxima de giro;
- a avaliação é uma multiplicação, um truncamento e uma interpolação, então é determinística e não aloca memória.

Os testes conferem a monotonicidade em 20.000 pontos para várias combinações de faixas.

### Por que Fritsch–Carlson nas curvas personalizadas

Os pontos definidos pelo usuário são saneados (ordenados, limitados e tornados não decrescentes) e interpolados com uma spline cúbica de Hermite monotônica. Diferente de uma spline cúbica comum, ela não ultrapassa os pontos, então uma curva personalizada nunca inverte de direção.

## Motor em tempo real

- Uma thread dedicada, com prioridade MMCSS "Games" (alternativa: `THREAD_PRIORITY_HIGHEST`).
- Ritmo: `Spin`, `Hybrid` (timer de alta resolução e depois espera ativa nos últimos ≤150 µs) ou `Timer`. O padrão é Híbrido a 125 µs. A 250 µs o motor só via cerca de 2,3 mil das ~7,6 mil mudanças de estado por segundo do Tarantula.
- Atrasos do agendador (mais de um intervalo de atraso) são contados, e o ritmo é ressincronizado em vez de compensar com rajadas.
- **Nenhuma alocação, lock ou liberação de memória na thread de entrada** depois do início. Os perfis são publicados por troca de ponteiro com `std::atomic`. A thread de entrada coloca os perfis aposentados numa fila SPSC, e a thread de controle libera a memória deles.
- A telemetria (original, saída, ns de processamento/envio/total, contexto, flags) vai para uma fila SPSC de 64 mil entradas. Se ela estiver cheia, a amostra é descartada e contada; a thread de entrada nunca espera.
- Os logs da thread de entrada usam um buffer circular separado e sem lock (`Logger::logRt`). A thread de escrita transforma os registros em JSON Lines.
- **Desconexão**: estado neutro enviado na hora, processamento reiniciado, nova tentativa de conexão a cada 250 ms.
- **Pass-through**: a flag de bypass (interface, Ctrl+Alt+P ou segurar Back+Start) envia o estado original sem alteração.
- **Encerramento seguro**: `stop()` sempre envia o estado neutro, fecha a saída (removendo qualquer controle virtual) e restaura a prioridade da thread.
- **Ciclos ociosos**: o XInput não informa nada enquanto o estado não muda. Se a suavização ou a transição ainda não estabilizou, o motor reprocessa a última amostra com um carimbo de tempo novo para a saída continuar convergindo.

## Como cada coisa é medida

| Grandeza | Como é medida | O que ela não é |
| --- | --- | --- |
| Intervalo entre amostras XInput | QPC no retorno do `XInputGetState` quando o contador de pacotes muda | não é o intervalo de polling USB: relatórios sem mudança são invisíveis para o XInput |
| Pacotes perdidos | lacunas no contador de pacotes do XInput entre duas leituras | não é perda no nível do USB |
| Taxa de relatórios HID | QPC ao receber o `WM_INPUT` de cada dispositivo (Raw Input, input sink) | inclui o agendamento do driver e da fila de mensagens. Relatórios entregues na mesma mensagem dividem o carimbo de tempo, e essas mensagens são contadas |
| Tempo de processamento | QPC em volta de `Processor::process()` | arredondado para 100 ns. A média do lote dá o valor preciso |
| Caminho do software | carimbo da leitura → retorno do envio | não inclui USB, driver, jogo nem monitor |
| Atraso adicionado pelo app (Painel) | metade do intervalo entre leituras (em média o estado novo espera isso) + leitura → envio; o pior caso usa o intervalo inteiro | não inclui o trecho controle virtual → jogo |

Os analisadores nunca assumem uma taxa nominal. O `TimingAnalyzer` estima a mediana a partir dos dados e marca as lacunas em relação a ela.

## Escolhas técnicas

- **C++20 + CMake + Ninja/MSVC**: como pedido. O núcleo também compila com `STICKYAIM_CORE_ONLY=ON`, sem nenhuma API do Windows.
- **Qt 6 Widgets** (6.8 LTS) em vez de QML: menos sobrecarga para muitas visualizações ao vivo desenhadas à mão e distribuição mais simples. Os gráficos são widgets `QPainter` próprios, então o Qt Charts não é necessário.
- **XInput** como fonte principal: o Tarantula 8K aparece como dispositivo compatível com Xbox 360 (xusb22), e o XInput expõe o contador de pacotes para detectar perdas.
- **Raw Input** para medir a taxa de relatórios e para controles que não são XInput. Uma janela só de mensagens com `RIDEV_INPUTSINK` recebe os relatórios mesmo sem foco. A leitura HID usa `HidP_*` com o descritor pré-analisado, então os eixos são lidos de forma genérica e mapeados pelos arquivos de dispositivo.
- **ViGEmBus** para a saída opcional: ainda é o único barramento de controle virtual amplamente usado e acessível pelo modo de usuário. Ele foi descontinuado pelo autor, por isso a saída é opcional e vem desligada ([DELIVERY.md](DELIVERY.md)).
- **nlohmann/json** para perfis, configurações, arquivos de dispositivo e relatórios: só cabeçalhos, mantido, licença MIT.
- **GoogleTest** para os testes.
- **Interface**: paleta escura no estilo das cores de sistema do macOS, fonte Segoe UI Variable, ícones SVG próprios e textos traduzidos por `T()` (`src/app/i18n.cpp`, `src/app/i18n_pt_br.cpp`).
