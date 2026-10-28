# Distribuição: instalador e códigos de ativação

## Gerar o instalador

```bash
powershell -ExecutionPolicy Bypass -File scripts/package.ps1
```

O resultado é **`dist\UpgradeAim-Setup-<versão>.exe`** (≈ 45 MB). O script:

1. Compila em Release numa pasta separada (`build\package`) e roda os testes. Isso não atrapalha o app aberto pelo `Upgrade Aim.bat`, que usa `build\release`.
2. Monta `dist\stage` com o `UpgradeAim.exe`, as DLLs do Qt, os perfis, os dispositivos e as licenças de terceiros.
3. Baixa os instaladores oficiais do ViGEmBus 1.22.0 e do HidHide 1.5.230 e confere o SHA-256 e a assinatura digital de cada um.
4. Compila `installer\UpgradeAim.iss` com o Inno Setup 6 (`winget install JRSoftware.InnoSetup`).

Ficam **fora** do instalador, de propósito:
- `upgradeaim-cli.exe`, porque ele consegue usar o controle virtual sem código de ativação;
- o gerador de códigos, porque ele usa a sua chave privada.

### O que o instalador faz no PC do cliente

- Mostra a página "Antes de instalar" (`installer\antes_de_instalar.txt`), com o aviso de risco para a conta.
- Instala o app em `C:\Program Files\Upgrade Aim`, com atalho no menu Iniciar e, se o cliente quiser, na área de trabalho.
- Instala o Microsoft Visual C++ Runtime de forma silenciosa. É o runtime padrão da Microsoft, não um driver.
- Opções marcadas por padrão: ele abre os instaladores do **ViGEmBus** e do **HidHide** com as janelas normais deles, e o cliente confirma. Nenhum driver é instalado escondido. Os instaladores ficam em `drivers\`, na pasta do app, para reinstalar depois.
- O desinstalador remove o app. Ele não remove o ViGEmBus nem o HidHide: o cliente desinstala os dois pelas Configurações do Windows, se quiser.

## Códigos de ativação

### Como funciona

- Na primeira vez que abre, o app mostra a **tela de ativação** com o **ID da máquina** (ex.: `825C-A5A8-45FB-86BF`) e um campo para o código.
- O cliente te envia o ID, você gera o código e manda para ele, e ele cola e clica em **Ativar**.
- O código é assinado com **ECDSA P-256** (criptografia do próprio Windows). O app só tem a chave **pública**, que serve para conferir. Ninguém consegue criar um código válido sem a sua chave **privada**.
- A ativação fica em `%APPDATA%\UpgradeAim\license.key`. Em Configurações → Sobre aparece a situação da licença e o ID da máquina.
- Sem um código válido, o app não passa da tela inicial, nem com `--lang` ou `--screenshots`.

### Gerar um código

Dê dois cliques em **`Gerador de codigos.bat`**, na pasta do projeto:

| Campo | O que colocar |
| --- | --- |
| Cliente | nome ou e-mail, só para o seu registro |
| ID da máquina | **obrigatório**. Na tela de ativação, o cliente clica em **Copiar pedido de ativação** e te manda a mensagem pronta ("Olá! Quero ativar o Upgrade Aim. O ID da minha máquina é 825C-…"). Cole a mensagem inteira: o gerador separa o ID sozinho. O código só funciona naquele PC |
| Tempo de uso | um número e a unidade (**minutos**, **horas** ou **dias**), ou **permanente** |

Embaixo aparece a hora exata em que o código vai expirar se você gerar agora. **O tempo conta a partir do momento em que você gera**, não da ativação. Clique em **Gerar código** e depois em **Copiar**. Todo código emitido fica registrado em `%APPDATA%\UpgradeAim Keygen\issued.csv` (número, data e hora, cliente, máquina, expiração, código).

Pela linha de comando (sem duração = permanente):

```bash
dist\keygen\upgradeaim-keygen.exe --issue --machine 825C-A5A8-45FB-86BF --hours 2 --name "Teste do Cliente X"
```

Também aceita `--minutes N` e `--days N`, que podem ser somados.

### Códigos de teste: o que acontece quando o tempo acaba

- **Com o app aberto:** ele confere a licença a cada 30 segundos e avisa 5 minutos antes de acabar. Quando acaba, para o motor, centraliza a saída, remove o controle virtual e volta para a tela de ativação com "Este código de ativação expirou." Para voltar a usar, só com um código novo.
- **Ao abrir o app:** com o código expirado, ele não passa da tela de ativação.
- **Trava na máquina:** o primeiro código que expira num PC fica registrado nele (no registro do Windows, em `HKCU\Software\UpgradeAim\License`, e em `%LOCALAPPDATA%\UpgradeAim\state.dat`). Mesmo que a pessoa atrase o relógio, o app mostra "Este código de teste já expirou neste computador." Reinstalar o app não apaga esse registro.
- **Relógio atrasado:** o app guarda a hora mais avançada que já viu naquele PC. Se o relógio voltar para antes disso, os códigos de teste param com "A data e a hora deste PC foram atrasadas."
- O tempo restante aparece na barra lateral ("Teste: faltam 1 h 20 min"), na tela inicial e em Configurações → Sobre.
- Códigos permanentes não dependem do relógio.

Verificado em 28/10/2026 neste PC:
- um código de 2 minutos travou o app aberto, com o motor parado e a tela de ativação mostrando "expirou";
- um código válido com o número marcado como expirado neste PC foi recusado;
- um relógio 2 h antes da última hora vista foi recusado;
- um código de 1 hora válido abriu normalmente, mostrando "faltam 1 h 0 min".
### A sua chave privada

- Fica em **`%APPDATA%\UpgradeAim Keygen\private.key`** (104 bytes). Ela **não** está no projeto nem no GitHub (`*.key` está no `.gitignore`).
- **Faça backup** num lugar seguro, como um pendrive guardado ou um gerenciador de senhas. Se perder, os códigos já emitidos continuam funcionando, mas você não consegue gerar novos sem recompilar o app com uma chave nova. E aí os códigos antigos param de valer nas versões novas.
- **Nunca envie** o `private.key` a ninguém: com ele, qualquer pessoa gera códigos.
- A parte pública está em `src/platform/license_public_key.inc`. Ela pode ficar no GitHub.

### Limites (seja honesto com os clientes)

- A verificação é offline. Alguém com conhecimento técnico pode modificar o executável para pular a ativação. O sistema impede o compartilhamento casual de códigos, não a pirataria determinada.
- A validade usa o relógio do PC do cliente. Atrasar o relógio é detectado depois que o app já viu uma hora mais avançada naquele PC. Se a pessoa apagar o registro e o arquivo de estado e ainda atrasar o relógio antes de abrir o app, consegue estender um teste. É o limite de uma verificação sem servidor.
- Trocar a placa-mãe ou reinstalar o Windows muda o ID da máquina, e o cliente vai precisar de um código novo.

## "Esconder o controle dos jogos" (HidHide em um clique)

Em Configurações → Saída, o botão **Configurar**:
1. Pede permissão de administrador ao Windows.
2. Adiciona o `UpgradeAim.exe` instalado à lista de permitidos do HidHide.
3. Remove entradas antigas e qualquer entrada que esconderia o controle virtual (`VID_045E PID_028E`).
4. Esconde as interfaces HID e XInput do controle físico conectado.

**Desfazer** mostra o controle de novo para os jogos. O registro de cada execução fica em `%APPDATA%\UpgradeAim\logs\hidhide.log`.

Verificado em 28/10/2026, com o helper rodando como administrador: o jogo passa a ver só o slot XInput 1 (o controle virtual). O caminho pelo botão, com o aviso do Windows, não foi clicado durante o desenvolvimento.

## Licenças dos componentes incluídos

O instalador copia para `licenses\`:
- **Qt 6.8.3:** LGPL-3.0, com linkagem dinâmica. As DLLs `Qt6*.dll` podem ser trocadas pelo usuário, como a LGPL exige.
- **ViGEmClient:** MIT, linkado estaticamente.
- **ViGEmBus e HidHide:** redistribuídos sem modificação, nos instaladores oficiais assinados.
- **nlohmann/json:** MIT.

Mantenha essas licenças junto se você distribuir de outro jeito.

## Lembrete sobre o risco

Vender ou distribuir o Upgrade Aim não muda o risco para quem usa. O Call of Duty proíbe modificação de entrada no online, e o RICOCHET procura isso. O aviso aparece no instalador, na ativação do controle virtual e no README. Não remova esses avisos.
