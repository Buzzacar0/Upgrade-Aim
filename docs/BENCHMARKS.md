# Benchmarks

Todos os números foram medidos em 27/10/2026 no PC de desenvolvimento (Windows 11 Pro 10.0.26200, x64, plataforma AMD, MSVC 2022, compilação RelWithDebInfo) com um **GameSir Tarantula 8K** via USB no modo XInput. Refaça as medições no seu PC com os comandos indicados; números de outra máquina não valem para a sua.

## 1. Tempo do dispositivo (CLI `measure`, analógicos parados, 5 s, leitura spin)

```bash
upgradeaim-cli measure --seconds 5 --mode spin
```

| Grandeza | n | média | DP | p50 | p95 | p99 | máx. |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Intervalo entre mudanças de estado XInput | 37.878 | 132,0 µs (7.576 /s) | 33,7 µs | 124,5 µs | 169,5 µs | 258,5 µs | 426,9 µs |
| Intervalo entre relatórios HID via Raw Input (IG_00) | 37.878 | 132,0 µs (7.576 /s) | 37,9 µs | 124,5 µs | 225,5 µs | 277,5 µs | 465,5 µs |
| Custo de uma chamada `XInputGetState()` | 3,0 M | 1,61 µs | — | — | — | 2,75 µs | 179,5 µs |

- Pacotes pulados entre leituras: 0. Mensagens HID com mais de um relatório: 3 de 37.876.
- O contador de pacotes avança em repouso porque os valores do analógico variam cerca de ±20 unidades brutas em todo relatório. Meça também com o analógico em movimento: um controle com sensor mais silencioso mostraria bem menos mudanças XInput em repouso, e isso não significaria uma taxa de polling menor.

## 2. Ruído em repouso (CLI `calibrate`)

```bash
upgradeaim-cli calibrate --rest 4 --range 6 --out cal.json
```

| Analógico | n | desvio médio | DP | máx. \|v\| | máx. \|v − média\| | deadzone interna recomendada (sem / com calibração) |
| --- | --- | --- | --- | --- | --- | --- |
| Direito | 258 | (+0,0140, +0,0062) | (0,0002, 0,0002) | 0,0159 | 0,0007 | 0,019 / 0,010 |
| Esquerdo | 258 | (+0,0060, +0,0296) | (0,0002, 0,0002) | 0,0308 | 0,0007 | 0,037 / 0,010 |

Essa medição usou uma versão antiga da captura, que esperava cerca de 15 ms entre leituras; por isso o n é só 258. A captura agora faz leitura spin. Os valores recomendados incluem uma margem de 20 % sobre o maior desvio observado, com mínimo de 0,010.

## 3. Motor (CLI `run`, saída de pré-visualização, híbrido 125 µs, analógico mexido à mão, 6 s)

```bash
upgradeaim-cli run --profile profiles/tarantula8k_precisao.json --seconds 6 --mode hybrid --interval 125
```

| Grandeza | Resultado |
| --- | --- |
| Estados novos processados | ≈ 7.400 /s (44.435 em 6 s) |
| Pacotes do controle não vistos pelo motor | 873 no total (≈ 2 %; ≈ 0,3 % nos primeiros 4 s) |
| Atrasos do agendador (> 1 intervalo) | 1 |
| `Processor::process()` | p50 0,21 µs, p99 1,21 µs |
| Envio para a saída (pré-visualização) | p50 0,1 µs, p99 0,2 µs |
| Leitura → envio concluído | p99 1,5 µs |

Mesmo PC, sessão anterior, intervalo padrão de 250 µs: ≈ 2.300 estados novos/s, e a maior parte das mudanças de estado do controle não era vista. Por isso o padrão agora é 125 µs. O modo spin vê todo pacote que o driver expõe, ao custo de ocupar um núcleo.

Saída ViGEm (controle virtual), medida em 28/10/2026 com o ViGEmBus 1.22.0 e o HidHide 1.5.230 instalados, perfil upgrade, 4 s: ≈ 7.400 estados/s enviados ao controle virtual; envio p50 1,9 µs, p99 4,5–6,0 µs; leitura → envio concluído p99 4,5–6,5 µs.

## 4. Atraso adicionado pelo app (Painel, ao vivo)

O Painel mostra o tempo de resposta sem e com o app. "Sem o app" é o intervalo entre estados novos do controle. "Com o app" soma esse intervalo à espera pela próxima leitura (metade do intervalo entre leituras, em média), ao processamento, ao envio e à suavização do perfil.

| Grandeza | Resultado (perfil base, pré-visualização) |
| --- | --- |
| Sem o app | ≈ 0,134–0,140 ms |
| Com o app | ≈ 0,197–0,203 ms |
| Adicionado pelo app | ≈ 0,063 ms em média, pior caso (p99) ≈ 0,13 ms |
| Quadros por segundo da tela | ≈ 64 fps (antes da correção de 27/10/2026: ≈ 14 fps) |

A latência de USB, driver e jogo é a mesma nos dois casos e não é medida.

## 5. Custo do processamento (sintético, `benchmarkProcessing`)

| Condição | Média do lote por amostra | p50 / p99 por amostra |
| --- | --- | --- |
| PC ocioso, perfis de exemplo | 87–92 ns | 102 / 202 ns (resolução do QPC: 100 ns) |
| Todos os núcleos ocupados por outras threads (teste `Latency.ProcessingCostUnderLoad`) | 386 ns | 102 / 202 ns; máx. 18,9 ms (preempção de uma thread de teste com prioridade normal) |

## 6. Bateria de comparação sintética (CLI `compare`, 1000 Hz, contexto hip)

```bash
upgradeaim-cli compare --profile profiles/<nome>.json --context hip --out relatorio.json
```

Os relatórios JSON completos estão em [benchmarks/](benchmarks/). Eles foram gerados antes da troca de nome dos perfis, então os arquivos ainda usam os nomes antigos (baseline = base, precision = precisao, custom_scurve = curva_s, tracking = rastreio_suave). O atraso adicionado é o tempo até 50 % (degraus) ou o atraso da correlação cruzada (seno e microajuste) em relação ao sinal original. A razão de ganho é RMS(saída) / RMS(entrada) na direção do teste.

| Teste | Base | Precisão | Curva S | Rastreio suave (hip) | Rastreio suave (ADS) |
| --- | --- | --- | --- | --- | --- |
| ruído em repouso: RMS da saída (original 0,0257) | 0,000 | 0,000 | 0,000 | 0,000 | 0,000 |
| degrau 50 %: atraso adicionado | 0 ms | 0 ms | 0 ms | 0 ms (bypass de flick) | 0 ms |
| rastreio 1 Hz: atraso adicionado / ganho | 0 ms / 0,922 | 0 ms / 0,606 | 0 ms / 0,538 | **12 ms** / 0,477 | **12 ms** / 0,349 |
| microajuste: atraso adicionado / ganho | 0 ms / 0,528 | 0 ms / 0,163 | 0 ms / 0,142 | **13 ms** / 0,188 | **13 ms** / 0,115 |
| flick à esquerda: atraso adicionado | 0 ms | 0 ms | 0 ms | 0 ms | 0 ms |
| microajuste: RMS de jerk da saída (original 5,0e-3) | 4,2e-3 | 1,8e-3 | 1,3e-3 | 8,6e-5 | 5,3e-5 |

Como ler a tabela:

- Os ajustes estáticos (deadzone, curva, faixas, sensibilidade) adicionam **zero** de latência: a saída muda na mesma amostra que a entrada.
- Curvas convexas diminuem o ganho em deflexões pequenas (ganho no microajuste de 0,14–0,16 contra 0,53 na linear). Isso quer dizer mais curso do analógico por unidade de saída, que é a troca pretendida por mais precisão. Não quer dizer que o controle fica "mais lento" na deflexão total: degrau 100 % e círculo completo continuam com ganho 1,000.
- A suavização é o único recurso que adiciona latência. A primeira versão do perfil Rastreio suave adicionava 37–38 ms. Ela foi ajustada para 12–13 ms. Como o ruído em repouso medido neste controle é baixo, compare com o perfil Precisão antes de ficar com ele.
- O jerk de rastreio do perfil Base é maior que o original (1,2e-4 contra 2,5e-5) porque a deadzone cria uma quina a cada passagem pelo zero. É um efeito real e pequeno de qualquer deadzone.

## Reproduzindo

1. Compile em release (`scripts/build.ps1` ou `Upgrade Aim.bat`).
2. Rode os comandos acima com o controle conectado.
3. Rode `ctest --preset release -R Latency -V` para o teste sob carga.
4. Compare os seus relatórios JSON com os de [benchmarks/](benchmarks/). Os resultados sintéticos precisam bater até o último dígito (sinais com semente fixa). Os resultados de dispositivo e de tempo dependem do hardware e da carga.
