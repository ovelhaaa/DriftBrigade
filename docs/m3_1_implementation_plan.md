# M3.1 — Plano de implementação

## M3.1A — Listening Decision Record

| Item | Definição |
|---|---|
| Objetivo | Validar a sessão cega, explicar as divergências e registrar os estados das decisões. |
| Arquivos | Os três documentos M3.1 em `docs/`. Entradas de leitura nos exports e no pacote M3.0. |
| Dependências | Ponta da PR #11, pacote com gabarito e manifesto, baseline M2.9 da PR #10. |
| Riscos | Trocar a identidade de letras entre grupos, inferir modo por voto, confundir intensidade com confiança ou uma comparação contextual com efeito isolado. |
| Testes | Conferência cruzada CSV/Markdown/gabarito/manifesto, hashes, verificador do pacote, CTest DSP, teste de estado/roteamento e testes do Listening Lab com o artefato. |
| Aceitação | 12 grupos e 26 candidatos decodificados, comentários e notas coerentes, divergências e decisões adiadas documentadas, hashes dos exports idênticos e escopo restrito aos documentos. |

## M3.1B — Musical Architecture Freeze

| Item | Definição |
|---|---|
| Objetivo | Começar por um reteste offline de D com a mesma fonte e Chaos controlado. Congelar a referência Digital/Wander/Gentle e as cenas internas reproduzíveis. |
| Arquivos previstos | `tools/BBDListeningBakeoff.cpp`, `tools/BBDListeningFixtures.h`, verificador M3.0 se o esquema evoluir, testes do Listening Lab se o formato for estendido e os documentos M3.1. |
| Dependências | M3.1A e resolução da base de integração. Confirmar a ponta da PR #11 antes de empilhar trabalho ou usar o commit integrado quando estiver disponível. |
| Riscos | Confundir fonte, Chaos, profundidade percebida e algoritmo. Confundir ganho de apresentação com efeito. Tratar uma preferência de voz como nova exigência de interface. |
| Testes | Primeiro, duplicar a voz D02 em Chaos .5 e 1, com Wander, PaperNarrowband e PhaseDrift e demais macros fixas. Manter D01 como âncora de pad, trocar a ordem cega entre rodadas e registrar notas distintas de RAW e LEVEL_MATCHED. Conferir seed, excursão, envelope e regressão Digital. Depois, retestar A03 com ataque e cauda julgados separadamente, B01/B02 com mais fontes, C com nível e feedback cruzados na mesma fonte e E com o mesmo ataque em mais de uma cena. |
| Aceitação | Pacote de voz reproduzível, sem clamp, alocações ou guardas na renderização normal, gabarito conferível, julgamento por contexto e escolha explícita de manter ou promover PaperNarrowband dentro da pesquisa. Aprovar qualquer combinação de vencedores em escuta conjunta antes de congelá-la. |

## M3.1C — BBD Product Admission

| Item | Definição |
|---|---|
| Objetivo | Condicional à decisão musical. Implementar uma configuração interna com envelope explícito, política de fallback e ciclo de vida verificável. |
| Arquivos previstos | `Source/dsp/DriftEngine.h`, `Source/dsp/DriftEngine.cpp`, `Source/dsp/BBDModulatedDelayVoice.h`, `tools/BBDProductEnvelopeQualification.cpp`, testes de engine e estado. |
| Dependências | M3.1B, decisão de ganho/estágios, regra aprovada para Center/Depth fora da faixa, reset/prepare e eventual troca de backend. |
| Riscos | Clamp silencioso, mudança de significado de presets, pitch jumps, perda de cauda, preparação parcial, custo assíncrono e DC no loop. |
| Testes | Limites de clock/atraso por taxa e variante, automação cruzando pisos, entradas não finitas, falha de prepare e fallback, resets e caudas, contagem de alocações, guardas e excursões. Comparar feedback atual com polos candidatos em fontes de escala cheia e em graves. |
| Aceitação | Toda solicitação fora do envelope recebe resultado e motivo reproduzíveis. Estado de falha é consistente. Sem reconstrução pesada, alocações, locks ou arquivo no callback. Digital preserva regressão, parâmetros, IDs, defaults e presets. O modo BBD continua interno enquanto faltar validação em host. |

## M3.1D — Host Validation

| Item | Definição |
|---|---|
| Objetivo | Qualificar a configuração admitida em máquinas e hosts representativos antes de discutir distribuição. |
| Arquivos previstos | `tests/PluginTests.cpp`, harness de realtime em `tools/`, workflows de qualificação pertinentes e relatório em `docs/`. |
| Dependências | M3.1C e critérios de deadline e fallback aprovados para as plataformas alvo. |
| Riscos | Blocos pequenos e variáveis, cauda de latência de callback, automação por bloco, diferença entre bounce e playback, reprepare e restauração com taxa diferente. |
| Testes | Repetições em 44,1/48/88,2 kHz, blocos 16/32/64/128/512 e tamanhos variáveis, mono e estéreo. Testar bypass, automação rápida, Mix 0, feedback alto, silêncio e caudas, troca de taxa/buffer, offline e restore de presets. Exercitar taxas fora do envelope e falha de prepare. Registrar p99/p99.9, máximo, misses e configuração por máquina e host. |
| Aceitação | Margem repetível frente ao deadline nas combinações admitidas, com alvo de p99 até 50% do prazo conforme M2.9 e sem misses nos ensaios de admissão. Casos incompatíveis seguem a política aprovada. Confirmar estado, ausência de alocação, caudas limitadas e comportamento previsível em automação e bypass. Manter 96 kHz experimental até qualificação própria. |

O [reteste M3.1B.1](m3_1b_organic_voice_retest.md) e a [admissão M3.1C](m3_1c_bbd_product_admission.md) completaram os passos anteriores. A próxima etapa é a qualificação do modo BBD interno em host.
