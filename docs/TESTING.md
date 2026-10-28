# Testes

## Testes automáticos

```bash
ctest --preset release
```

São 82 testes em dois executáveis. Todos passaram em 27/10/2026.

| Área (requisito) | Testes |
| --- | --- |
| Monotonicidade das curvas e casos de borda | `Curve.*` (todos os tipos de curva, varreduras de 10.000 pontos, entradas NaN e fora do domínio, saneamento dos pontos personalizados), `ResponseLut.*` (monotônica e limitada em 16 combinações de faixas fina e rápida, continuidade entre faixas) |
| Comportamento da deadzone | `Deadzone.*` (radial, axial, nenhuma; limites exatos; continuidade na borda interna; saturação externa; validação) |
| Simetria entre eixos | `Pipeline.AxisSymmetry`, `Deadzone.SymmetricInAllQuadrants` (idêntico bit a bit em ±X, ±Y e X↔Y) |
| Saturação e normalização | `Normalization.ExtremesAndSymmetry` (ida e volta int16 sem perda, tratamento de −32768, NaN), `Pipeline.SensitivityAndCircularSaturation`, `Pipeline.OutputAlwaysWithinRange` |
| Ruído e drift da entrada | `RestAnalysis.*` (estimativas de desvio, ruído e variação; a deadzone recomendada suprime 100 % das amostras em repouso, com e sem calibração de centro; aviso de drift), `RangeAnalysis.*`, `DriftMonitor.*`, `Analysis.DeadzoneSuppressesRestNoise` |
| Variação no tempo entre amostras | `Timing.MeasuresIntervalAndJitter` (variação do intervalo de √2·σ), `Timing.DetectsDroppedSamplesAndGaps`, `Timing.NoNominalRateAssumed` |
| Desconexão do controle | `Engine.DisconnectNeutralizesOutputAndReconnects`, `Engine.StartsWithoutDeviceAndWaits` |
| Persistência da configuração | `Config.*` (ida e volta JSON sem perda, gravação atômica, carregamento tolerante, rejeição de arquivos ruins, perfis de exemplo carregam e compilam), `DeviceDb.*`, `Recording.CsvRoundTrip` |
| Latência do processamento sob carga | `Latency.ProcessingCostUnderLoad` (todos os núcleos ocupados), `Analysis.StaticProcessingAddsNoLatency`, `Analysis.SmoothingLagIsMeasured` (atraso do EMA = τ·ln2 ± 1 amostra) |
| Comportamento do motor | aplicação de perfil, pass-through, combo de pânico, contagem de pacotes perdidos, troca de perfil sob carga (300 trocas recebendo entrada), tempo da telemetria, reprodução em tempo real |
| Suavização | identidade quando desligada, constante de tempo do EMA e independência da taxa de amostragem, adaptação do One Euro, bypass de flick, reinício em lacunas e em tempo não monotônico |
| Determinismo | `Pipeline.DeterministicForIdenticalInput` (saída idêntica bit a bit com suavização, transição, ruído e variação de tempo) |

## Comparando o controle original com o processado

Separe dois tipos de evidência:

- **Objetiva**: o que o processamento faz com o sinal (atraso, ganho, ruído que passa, resolução, suavidade, saturação). É medida pelo Upgrade Aim e se repete.
- **Subjetiva**: como a mira parece ou rende no jogo. Ela é afetada pelo processamento e pelo aim assist do próprio jogo, pelo aprendizado e pela expectativa. Só dá para estimar com testes controlados.

### A. Comparação objetiva (repetível)

1. Testes → Comparação sintética → escolha a referência "Controle original (sem processamento)" (ou outro perfil) → Rodar. Os sinais são gerados com semente fixa, então o mesmo perfil sempre dá os mesmos números.
2. Testes → Captura ao vivo: escolha um tipo de movimento, faça o movimento, e a mesma gravação é repassada pelo perfil ativo e pela referência. Isso isola o efeito do perfil da variação da sua mão.
3. Lab. de latência: confira que o intervalo entre amostras, os pacotes perdidos e o tempo de processamento não mudam entre perfis. O custo de processamento quase não depende do perfil (87–92 ns medidos).
4. Exporte o relatório em JSON.

O que observar:

| Objetivo | Métrica |
| --- | --- |
| Sem drift ou variação em repouso chegando ao jogo | RMS da saída em ruído em repouso = 0, fração não zero = 0 |
| Microcorreções mais finas | ganho no microajuste abaixo de 1 com muitos níveis de saída (sem perder resolução) |
| Sem atraso adicionado | atraso adicionado de 0 ms nos testes de degrau, rastreio e flick; qualquer valor > 0 vem da suavização |
| Saída mais suave | RMS de jerk menor, mas só junto com um atraso aceitável |
| Velocidade máxima ainda alcançável | ganho 1,000 nos testes degrau 100 % e círculo completo |

### B. Comparação subjetiva (A/B às cegas, num modo privado ou offline)

1. Escolha dois perfis, A e B (por exemplo *Base* e *Precisão*). Eles ficam nos atalhos Ctrl+Alt+1 e Ctrl+Alt+2 conforme a ordem na lista de perfis.
2. Peça para outra pessoa trocar o perfil entre as rodadas sem te contar qual está ativo (lista de perfis do Painel ou atalhos). Se ninguém puder ajudar, sorteie com um dado e só confira o perfil depois.
3. Use um treino fixo e repetível: mesmo mapa, mesmos alvos, mesma arma e acessórios, mesma sensibilidade no jogo, pelo menos 10 rodadas por perfil, em ordem alternada aleatória.
4. Anote por rodada: o perfil (preenchido depois), uma nota de 1 a 5 e uma medida objetiva do jogo, se houver (tempo para abater um alvo fixo, acertos em N tiros).
5. Escreva as condições e os resultados em Testes → Notas subjetivas e exporte. O relatório mantém essas notas separadas das medições.
6. Só conclua que um perfil é melhor se a diferença for consistente entre as rodadas e maior que a variação de uma rodada para outra.

As configurações de deadzone e curva do próprio jogo se somam às do Upgrade Aim. Nas comparações com saída virtual, deixe o jogo com a menor deadzone e com curva linear, se possível, e não mude isso entre A e B.
