# Solução de problemas

Logs: `%APPDATA%\UpgradeAim\logs\app.jsonl` (app) e `cli.jsonl` (CLI), em JSON Lines com carimbos de tempo monotônicos. Configurações → "Abrir pasta de dados".

## Controle

**O controle não aparece / o Painel mostra "Sinal de demonstração".**
- Rode `upgradeaim-cli devices`. Se nenhum slot XInput estiver conectado, confira a chave de modo do controle (ele precisa estar no modo XInput/PC para a fonte XInput) e o cabo ou receptor.
- Se ele aparece em HID mas não em XInput, escolha a entrada HID como fonte e adicione um mapeamento de dispositivo ([ADDING_CONTROLLERS.md](ADDING_CONTROLLERS.md)).
- Se o HidHide estiver instalado e o dispositivo escondido, adicione o `UpgradeAim.exe` à lista de permitidos do HidHide.

**Um analógico mexe o círculo errado, ou os dois círculos.**
- No Painel e no Lab. do analógico, o círculo da esquerda é o analógico esquerdo e o da direita é o direito, cada um com o valor original (cinza) e o processado (azul ou laranja no ADS). Até 27/10/2026 o Lab. do analógico mostrava o analógico direito duas vezes (original e processado), o que dava a impressão de que o direito mexia os dois.

**"Pacotes perdidos" não para de subir.**
- O motor lê com menos frequência do que o controle muda de estado. Use Lab. de latência → Modo "Spin", ou baixe o intervalo para 125 µs (o padrão). A 250 µs cerca de 70 % das mudanças de estado do Tarantula 8K não eram vistas.
- Algumas centenas por minuto no modo Híbrido é normal (variação do agendador do Windows). Isso quer dizer que estados intermediários foram pulados, não que o estado mais recente se perdeu.

**As amostras novas por segundo ficam muito abaixo da taxa de polling anunciada.**
- O XInput só informa *mudanças* de estado. Um analógico totalmente parado não gera nenhuma. Meça mexendo o analógico.
- A tabela Raw Input do Lab. de latência mostra a chegada dos relatórios HID separadamente. Taxa de polling USB, taxa de relatórios HID e taxa de processamento do Upgrade Aim são três coisas diferentes.

**O analógico tem drift ou a mira anda sozinha parada.**
- Lab. do analógico → "Teste em repouso · 3 s" (sem tocar) → "Aplicar centro" e depois "Aplicar deadzone". O Painel mostra a calibração de centro ativa.
- O aviso "possível drift no analógico" quer dizer que o desvio em repouso passa de 0,06. A deadzone recomendada ainda esconde isso, mas o analógico pode precisar de limpeza ou manutenção.

**Os analógicos na tela parecem lentos ou travando.**
- Painel → "Taxa de leitura do controle" mostra quantos quadros por segundo a tela desenha. A interface redesenha uma vez por atualização do monitor. Nenhum monitor mostra 8 kHz, mas o motor continua lendo e processando todos os estados.
- Até 27/10/2026 o Painel procurava de novo todos os dispositivos HID em todo quadro (cerca de 60 ms cada vez), o que limitava a tela a uns 14 fps. Agora essa busca só acontece ao iniciar, ao conectar/desconectar o controle e em "Procurar".

**As diagonais parecem puxadas para os eixos.**
- O tipo de deadzone está como Axial. Use Radial.

## Processamento

**A mira parece atrasada.**
- Confira a suavização: qualquer filtro diferente de "Desligada" adiciona atraso. O card "Suavização" do Painel e a página Testes → Sintético mostram o atraso exato (rastreio 1 Hz, microajuste). Aumente o corte mínimo do One Euro ou a velocidade do bypass de flick, ou desligue a suavização.
- Confira se há transição hip ↔ ADS em Perfis de mira.
- Confira as deadzones e curvas do próprio jogo e do Steam Input: elas se somam às do Upgrade Aim.

**A deflexão total não chega na velocidade máxima de giro.**
- O limite externo está alto demais para o alcance do seu analógico. Rode o "Teste de alcance · 6 s" e aplique a recomendação.
- A sensibilidade está abaixo de 1 nesse eixo.

**Um perfil não carrega.**
- A mensagem de erro diz qual campo é inválido (por exemplo, deadzone interna ≥ limite externo). Os arquivos são validados, não "consertados" sem avisar.

## Saída virtual

**"ViGEmBus driver not found".**
- O driver não está instalado ou não está rodando. Veja [DELIVERY.md](DELIVERY.md). Compatibilidade → Drivers mostra a situação dele.

**"O controle virtual ficou no mesmo slot XInput da fonte de entrada".**
- Reconecte o controle físico primeiro e só depois ative a saída virtual, para o controle físico ficar com o slot menor.

**O jogo responde em dobro / de forma errática.**
- Entrada dupla: o jogo lê os dois controles. Esconda o controle físico com o HidHide (com o Upgrade Aim na lista de permitidos), ou configure o Steam Input adequadamente ([COMPATIBILITY.md](COMPATIBILITY.md)).

**Sem vibração.**
- A vibração é repassada do controle virtual para o slot XInput do controle físico. Ainda não foi verificado em hardware real. Confira se o controle físico aceita vibração pelo XInput.

## Desempenho

**Um núcleo do processador em 100 %.**
- Os modos Spin e Híbrido com intervalos curtos fazem espera ativa de propósito. Use o modo Só timer (menos CPU, mais variação) ou um intervalo maior. A troca aparece no Lab. de latência.

**Os atalhos não funcionam.**
- Outro programa já registrou Ctrl+Alt+P/X/1–9. O app avisa isso ao abrir.

## Compilação

**`vswhere.exe not found`.** Instale o Visual Studio 2022 ou o Build Tools 2022 com a carga de trabalho de C++.

**`Qt 6 (msvc2022_64) not found`.** Passe `-QtRoot C:\Qt\6.8.3\msvc2022_64` ou defina `QT_ROOT`.

**O download do FetchContent falha.** A primeira configuração precisa de acesso ao GitHub. Atrás de um proxy, defina `HTTPS_PROXY`.

**O `UpgradeAim.exe` não abre fora da pasta de compilação (faltam DLLs do Qt).** Compile com `-Deploy` ou rode o `windeployqt` no executável.

**Meus perfis e configurações sumiram depois da troca de nome.** A pasta de dados mudou de `%APPDATA%\StickyAimLab` para `%APPDATA%\UpgradeAim`. Na primeira execução o app copia a pasta antiga para a nova, se a nova ainda não existir. Se você já tinha aberto a versão nova antes, copie manualmente os arquivos da pasta antiga.
