# M3.1B.1 — Reteste controlado da modulação na voz

O experimento verifica se a preferência por PaperNarrowband registrada na voz D02 reaparece quando Chaos passa de 100% a 50%. D01 usava pad e Chaos moderado. D03 usa a voz de D02 em 50% e D04 replica D02 em 100% com novos rótulos. Cada grupo compara as três formas nativas de Wander, PaperNarrowband e PhaseDrift.

| Controle | Valor comum |
|---|---:|
| Motion | 0,7 Hz |
| Depth | 0,25 |
| Center | 12 ms |
| Coherence | 0,3 |
| Dynamics | 0,15 |
| Feedback | 0,10 |
| Mix | 0,50 |
| Width | 0,95 |

Os grupos são cópias da definição D02 com Chaos como única diferença nos parâmetros. Dynamics permanece ativo e a modulação responde ao Chaos efetivo do motor, que inclui influência de envelope e é limitado a 1. A fonte C mono duplicada em estéreo, a seed do motor 77, a seed BBD 570 e a seed da fonte permanecem as da M3.0. A fixture usa ExperimentalBBD Balanced 1024, Nominal, Gentle, filtros Table1, caráter sintético M2.2, transferência polinomial de força 1, compander 0,47 µF/10 kΩ, ExternalWetReturn e bloqueio DC de saída de 5 Hz.

O manifesto contém as seis renderizações em RAW e LEVEL_MATCHED, a referência DRY_C e telemetria por candidato. O gabarito separado vincula as letras às variantes. A seed cega é `0x4d334231`, com embaralhamento por grupo e rejeição de ordens duplicadas ou iguais à de D02. Os WAVs têm 480.000 quadros, estéreo PCM24 a 48 kHz. O RMS estéreo ativo de 0,5 a 7,5 s define um ganho constante para cada arquivo. O teto de pico do matching é 0,98.

O renderizador verifica o limite analítico de cada variante, a excursão integralmente admitida e o teto de clock `min(384000, 8 * Fs)` com até 16 eventos por amostra e voz. Compara áudio e trajetória de repetições da mesma variante. O relatório registra RMS e pico da modulação, atrasos, clocks, picos, RMS, correlação, clamps, guardas e alocações. O verificador independente mede o PCM, confere o ganho constante e cruza o gabarito, o manifesto e o scorecard. Quando fornecido o pacote M3.0 da mesma toolchain, compara a referência seca e cada WAV RAW de D04 com D02 por identidade.

Siga o guia de audição do pacote. Julgue naturalidade, periodicidade, estabilidade de pitch, integração e utilidade musical antes de revelar. PaperNarrowband nos dois regimes reforçaria a preferência para essa voz. Uma escolha diferente em D03 com repetição em D04 indicaria dependência do regime. Se D04 também divergir da primeira sessão, considere ordem de apresentação e variabilidade da preferência. Empates ou votos inconsistentes pedem mais audição. RMS de modulação e excursão observada podem ajudar a interpretar escolhas, mas formas nativas e diferenças de intensidade percebida fazem parte do teste. Uma voz sintética, uma seed e uma sessão não sustentam uma promoção de variante para produção.

## Geração e dependências

Base `98767960ad91c2173d7b7a503d33aa3e8c594a72` da PR #11 aberta em rascunho. O main remoto está em `dedf4cb76e15a335159cb4c227f66b9ae69224bb`. A branch de trabalho mantém os documentos M3.1 e o Listening Lab que estavam presentes no workspace. Para usar em outro checkout, leve a pasta completa `tools/listening-lab/`, que ainda não fazia parte da base M3.0.

```powershell
cmake --build build-dsp --target drift_bbd_listening_bakeoff
$package = "output/DriftBrigade-M3.1B1-Organic-Voice-Retest"
./build-dsp/drift_bbd_listening_bakeoff --voice-retest $package
./build-dsp/drift_bbd_listening_bakeoff --voice-retest output/m3_1b1_repeat
python tools/verify_bbd_listening_bakeoff.py $package --voice-retest --compare output/m3_1b1_repeat
python -m http.server 8080 --bind 127.0.0.1 --directory tools/listening-lab
```

Abra `http://127.0.0.1:8080` e selecione a pasta do pacote. A reprodução usa o mesmo mecanismo de sincronização da M3.0. Use uma pasta de saída dedicada a cada geração.

## Resultado da validação local

A excursão solicitada e admitida foi de 1,06693 ms nos seis candidatos. O clock máximo observado foi 52.303,12 Hz, com até três eventos por amostra e voz. O pico PCM máximo foi 0,089152 e a diferença máxima de RMS dentro de um grupo após matching foi 7,97 × 10⁻⁸ dB. Houve zero clamps ocultos, guardas e alocações no processamento instrumentado. Os 13 hashes WAV coincidiram em duas renderizações. O dry e os três RAW de D04 coincidiram com D02 por identidade na mesma toolchain.

Passaram 26/26 testes CTest DSP, 1/1 teste de estado e roteamento do plugin e 5/5 testes Node do Listening Lab com os dois pacotes. A sanidade curta verifica também 44,1 e 88,2 kHz. A validação de carregamento do Lab foi automatizada. A audição e a validação de execução em CI ficam pendentes.

[PR #11](https://github.com/ovelhaaa/DriftBrigade/pull/11) · [Decisões da M3.1](m3_1_listening_decisions.md).

## Decisão após a audição

**RETAIN — Wander permanece como modulação padrão do DriftBrigade**, por decisão do responsável pelo produto. No reteste da voz, PhaseDrift recebeu os dois votos, ambos de intensidade 4/5, com Chaos em 50% (D03) e 100% (D04). A preferência por PaperNarrowband da sessão D02 não se repetiu em D04, cujos RAW coincidem com D02 por variante. PhaseDrift ocupou a letra Y nos dois grupos e o scorecard não identifica o modo de escuta nem inclui comentários. A escolha de uma variante específica para voz fica em RETEST.
