# DRIFT BRIGADE — Guia de Teste da Versão Beta (M3.2.1 Friends Beta)

Obrigado por testar a versão beta do **DriftBrigade** (`v0.2.0-beta.1`)!
Esta versão traz o motor digital multibanda congelado (**DigitalFractional / Wander / Gentle**) com a nova identidade visual **Precision Instrument** (Off-White + Grafite + Laranja), o visualizador **Motion Field** e os três pontos de partida **Quick Starts** (`SUBTLE`, `WIDE`, `DRIFT`).

O objetivo desta rodada é avaliar o plugin em contextos reais de produção musical, mixagem e sound design.

---

## 1. Instalação Rápida

1. Copie a pasta `VST3/DRIFT BRIGADE.vst3` do pacote para `C:\Program Files\Common Files\VST3\`.
2. Alternativamente, execute `Standalone/DRIFT BRIGADE.exe` para testar diretamente com sua interface de áudio.
3. Na sua DAW, carregue **DRIFT BRIGADE** (fabricante: **Drift Audio**) em pistas mono ou estéreo.

> **Nota sobre assinatura digital:** Como esta é uma build beta privada não assinada digitalmente, o Windows pode exibir um aviso informativo ao abrir o aplicativo Standalone pela primeira vez. Nenhuma telemetria, coleta de dados ou conexão de rede é realizada pelo plugin.

---

## 2. Roteiro Sugerido de Avaliação (15 a 20 minutos)

1. **Primeira Impressão e Quick Starts:**
   - Insira o plugin em um pad, guitarra limpa, piano elétrico (Rhodes/Wurli), synth ou vocal.
   - Alterne entre **SUBTLE**, **WIDE** e **DRIFT** no topo direito da janela.
   - Os três pontos de partida fazem sentido musicalmente logo de cara?
2. **Exploração dos Controles (`MOVEMENT`, `SPACE`, `CHARACTER`):**
   - Ajuste **Motion**, **Depth** e **Chaos** para sentir a diferença entre modulação senoidal regular (`Chaos 0%`) e deriva orgânica (`Chaos 55–100%`).
   - Explore **Center** (`0.3 ms` a `30 ms`), **Coherence** (bandas travadas vs. independentes) e **Width** (abertura entre L e R).
   - Teste **Dynamics**, **Feedback** e **Mix** (incluindo `Mix = 0%` e automação na DAW).
   - Teste o **duplo clique** nos knobs para voltar ao padrão e o **arraste fino** segurando `Ctrl` ou `Alt`.
3. **Visualizador `MOTION FIELD`:**
   - Observe se as quatro trajetórias e o escopo estéreo ajudam a compreender o que os controles estão fazendo sem distrair a audição.
   - Teste o botão **LIVE / HOLD** e o redimensionamento da janela pelo canto inferior direito.
4. **Estabilidade na Sessão:**
   - Salve o projeto na DAW, feche e reabra para confirmar que os parâmetros retornam exatamente como foram deixados.
   - Renderize (bounce/export offline) um trecho da faixa e verifique se o resultado soa igual ao playback em tempo real.

---

## 3. O Que Gostaríamos de Saber (Perguntas Rápidas)

Você pode responder de forma livre ou usar os tópicos abaixo:

- **Qualidade Sonora:** O movimento soa musical e orgânico? Como se comporta nos graves, médios e agudos?
- **Aplicações Musicais:** Em quais instrumentos ou buses o DriftBrigade funcionou melhor (e onde não funcionou)?
- **Facilidade de Uso e Clareza dos Controles:** Foi fácil encontrar o som desejado? Algum controle pareceu confuso ou com faixa pouco útil?
- **Qualidade da Interface e Motion Field:** A leitura dos valores, contraste, tamanho dos controles e resposta visual do Motion Field foram confortáveis na sua tela?
- **Uso de CPU:** O consumo de CPU na DAW foi leve, tanto com a janela do plugin aberta quanto fechada?
- **Sugestões para a Próxima Versão:** O que mais fez falta durante o uso real?

---

## 4. Modelo para Reportar Problemas ou Bugs

Caso encontre qualquer erro visual, clique de áudio, problema de estado ou travamento, envie as informações abaixo:

```text
DAW e versão: (ex.: REAPER 7.81 / Ableton Live 12.1 / Studio One 6)
Versão do Windows: (ex.: Windows 11 23H2 / Windows 10 22H2)
Versão/Commit do Plugin: (ver arquivo VERSION.txt do pacote, ex.: 0.2.0-beta.1)
Formato utilizado: (VST3 ou Standalone)
Interface de áudio e driver: (ex.: Focusrite ASIO / RME ASIO / WASAPI)
Sample Rate e Buffer Size: (ex.: 48000 Hz / 128 samples)
Escala de tela do Windows: (ex.: 100%, 125%, 150%, 200%)

Passos para reproduzir:
1.
2.
3.

Comportamento observado:
(Descreva o que aconteceu e, se possível, anexe uma captura de tela ou trecho de áudio.)
```
