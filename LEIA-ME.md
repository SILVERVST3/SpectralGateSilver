# SpectralGate SILVER

Gate com **detector espectral**, inspirado no Harrison Spectral Gate.
O plugin aprende o espectro do som que você quer manter (**Source**, linha verde) e do
vazamento que quer cortar (**Spill**, linha vermelha). Só a energia que fica **acima da
linha vermelha e abaixo da linha verde** (pintada de verde na tela) consegue abrir o gate.

## Como compilar pelo GitHub

1. Crie um repositório novo e envie pelo site: `CMakeLists.txt`, `build.yml`, `LEIA-ME.md` e a pasta `Source`.
2. Como o site não envia a pasta oculta `.github`, crie o workflow à mão:
   **Add file → Create new file**, digite no nome `.github/workflows/build.yml`
   e cole o conteúdo do arquivo `build.yml` (já está com `runs-on: windows-2022`).
3. Faça o commit. Em **Actions**, espere terminar (~10 min) e baixe o artefato
   `SpectralGate-SILVER-VST3-Windows`.
4. Copie a pasta `SpectralGate SILVER.vst3` para `C:\Program Files\Common Files\VST3\`
   e faça um rescan no REAPER.

## Como usar (exemplo: microfone de caixa com vazamento do bumbo)

1. Toque um trecho em que só a **caixa** aparece e ligue **LEARN SOURCE**; deixe ligado
   um pouco para pegar a cauda e desligue antes do bumbo entrar.
2. Toque um trecho só com o **vazamento** (bumbo, chimbal) e ligue **LEARN SPILL**;
   desligue antes da caixa voltar. Se precisar, repita.
3. Ajuste o **Threshold** arrastando no medidor THRESH: ele mostra só o sinal "verde".
   O medidor fica verde quando passa do threshold (gate abre) e vermelho quando não passa.
4. Ajuste **Hold**, **Release** e **Depth** a gosto.

Os 24 nós de cada linha podem ser arrastados com o mouse:
- **Shift** + arrastar = ajuste fino
- **Ctrl** + arrastar = move a linha inteira
- **Duplo clique** no nó = volta ao padrão
- **CLEAR** apaga tudo e volta as linhas ao padrão (sem aprendizado = gate comum)

## Controles

| Controle | Função |
|---|---|
| Threshold | Nível do sinal Source que abre o gate (−80 a 0 dB) |
| Hysteresis | O gate só fecha quando o sinal cai X dB abaixo do threshold |
| Attack | Tempo de abertura (0,01 a 50 ms) |
| Hold | Tempo que o gate fica aberto depois que o sinal cai (0 a 1000 ms) |
| Release | Tempo para chegar à redução total depois de fechar (5 a 3000 ms) |
| Depth | Redução máxima com o gate fechado (0 dB = sem efeito, −80 = −inf) |
| Mix | Mistura paralela original/com gate |
| Output | Ganho de saída |
| EXT SIDECHAIN | Usa a entrada de sidechain (pinos 3/4 no REAPER) para a detecção |
| BYPASS | Bypass momentâneo: segure para comparar |
| SIZE | Tamanho da janela (75 a 150 %); a janela também pode ser redimensionada pelo canto |

## Detalhes técnicos

- FFT de 1024 pontos (2048 em 88,2/96 kHz), janela Hann, salto de 1/8 da janela.
- Latência = tamanho da FFT (1024 amostras a 44,1/48 kHz). Isso funciona como *lookahead*:
  o gate já está aberto quando o transiente chega. O REAPER compensa a latência sozinho.
- Ao aprender, a linha Source fica 6 dB acima dos picos aprendidos e a Spill fica 3 dB acima.
- As linhas aprendidas ficam salvas no projeto junto com os parâmetros.
