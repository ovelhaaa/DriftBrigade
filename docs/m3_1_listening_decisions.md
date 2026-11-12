# M3.1A — Registro das decisões de audição

## Base e integridade

A referência é `98767960ad91c2173d7b7a503d33aa3e8c594a72`, ponta local e remota da M3.0. A PR #11 segue aberta como rascunho. O `main` remoto está em `dedf4cb76e15a335159cb4c227f66b9ae69224bb`, merge da PR #10 (M2.9). O `main` local em `03a5625` está atrás do remoto. Os arquivos M3.0 vêm da ponta da PR #11. A comparação de `Source/` entre essa ponta e o `main` remoto está vazia. [PR #11](https://github.com/ovelhaaa/DriftBrigade/pull/11) · [PR #10](https://github.com/ovelhaaa/DriftBrigade/pull/10).

O CSV e o Markdown concordam nos 12 grupos, votos, intensidades, 84 notas e 12 comentários, após normalizar escapes de Markdown e espaços finais. O CSV coincide com o modelo original em grupo, fonte, cena e nomes cegos. O gabarito de `output/DriftBrigade-M3.0-BBD-Listening-Bakeoff/ANSWER_KEY.csv` coincide byte a byte com o da renderização independente e foi conferido contra as 52 linhas processadas do manifesto, 26 por versão. SHA-256 antes e depois da análise:

| Arquivo | SHA-256 |
|---|---|
| LISTENING_SCORECARD.csv | `72f92871b41b3bde2486302d5ed02ee03813ecd9384c761721f37f228be6049d` |
| M3.0_LISTENING_RESULTS.md | `6e7feef64ccadd2bbd285fd2cff2605b1924e00f0568d3b2a10e5897ee033eef` |
| ANSWER_KEY.csv | `31d38a153f0f22cc55845fe199938721ef1762b77d43f885385b31f0ab17c5f2` |

O pacote contém RAW e LEVEL_MATCHED. O guia recomenda começar pelo LEVEL_MATCHED e o Listening Lab abre nesse modo. O export tem um voto por grupo e não registra a versão usada para cada voto. C01 menciona explicitamente LEVEL_MATCHED. Os demais comentários têm versão indeterminada e não há observação marcada explicitamente como RAW. As medições RAW abaixo são do manifesto. O casamento aplica ganho constante pelo RMS de 0,5 a 7,5 s, ajusta também ruído e cauda pelo mesmo fator e preserva diferenças de espectro e dinâmica local. [Guia](m3_0_listening_guide.md) · [Protocolo](m3_0_bbd_listening_bakeoff.md).

## Grupos decodificados

Notas na ordem naturalidade, estéreo, tom, transientes, cauda, artefatos e fadiga. Nos últimos dois critérios, 1 indica menor incidência. As notas são um vetor por grupo, sem atribuição independente a cada candidato. A intensidade descreve o voto registrado. Para leitura descritiva, 4–5 são fortes, 3 intermediário e 1–2 seriam fracos, sem votos nessa última faixa. B02 é inconclusivo para seleção mesmo com intensidade 3 registrada. Confiança na generalização é limitada por um ouvinte, três fontes sintéticas e uma audição documentada a 48 kHz.

| Grupo e cena | X | Y | Z | Voto | Notas | Impressão registrada |
|---|---|---|---|---|---|---|
| A01 Pad discreto | BBD Balanced | DigitalFractional | — | Y, 4/5 | 5/5/5/4/5/1/1 | Maior sensação de tamanho, modulação discreta e integrada. |
| A02 Pad amplo | DigitalFractional | BBD Balanced | — | Y, 4/5 | 5/5/5/5/5/1/1 | Movimento natural sem soar como chorus óbvio. |
| A03 Ataques e ecos | BBD Balanced | DigitalFractional | — | X, 3/5 | 3/3/3/3/3/2/1 | Perda de punch na cena. BBD mais interessante, sem atribuição inequívoca da perda a um backend. |
| A04 Deriva lenta na voz | DigitalFractional | BBD Balanced | — | X, 3/5 | 4/5/4/3/4/1/1 | Ambos agradam. Digital parece mais aberto. |
| B01 Pad estéreo | Balanced 1024 | Economy 512 | — | Y, 3/5 | 5/5/5/4/5/1/1 | Ambos agradam. Economy parece um pouco mais intenso. |
| B02 Dedilhado curto | Economy 512 | Balanced 1024 | — | Sem preferência, 3/5 registrado | 5/4/4/4/4/1/1 | Soam muito próximos. |
| C01 Pad baixo | Nominal | Conservative | — | Y, 4/5 | 5/4/5/4/5/1/1 | Mais movimento percebido no Conservative. Ambos parecem perder volume mesmo em LEVEL_MATCHED. |
| C02 Ataques e cauda | Conservative | Nominal | — | Y, 5/5 | 4/4/4/4/4/1/1 | Chiado aparente no Conservative motivou o voto no Nominal. |
| D01 Acordes amplos | PaperNarrowband | PhaseDrift | Wander | Z, 5/5 | 5/5/5/4/5/1/1 | Wander tem mais movimento e parece maior. |
| D02 Voz, Chaos alto | PhaseDrift | Wander | PaperNarrowband | Z, 4/5 | 4/4/5/4/4/1/1 | PaperNarrowband fica mais colado à fonte. PhaseDrift evidencia mais vozes. |
| E01 Pad amplo | Selective | Gentle | — | Y, 4/5 | 5/5/5/5/4/1/1 | Ambos agradam. Gentle é um pouco mais coeso. |
| E02 Ataques discretos | Gentle | Selective | — | Y, 4/5 | 4/4/4/4/4/1/1 | Selective traz mais movimento. |

## Matriz de decisões

| Família de teste | Grupos analisados | Preferência observada | Intensidade e confiança | Evidências qualitativas | Evidências objetivas relevantes | Divergências | Recomendação | Estado |
|---|---|---|---|---|---|---|---|---|
| A · Backend | A01–A04 | Digital no pad discreto e na voz lenta. BBD no pad amplo e nos ecos. | 4/5 nos dois pads, 3/5 nos outros. Forte por cena, sem vencedor geral. | Integração discreta e abertura com Digital. Movimento amplo natural e ecos interessantes com BBD. | Trajetórias de atraso idênticas por amostra, canal e banda. A comparação inclui toda a cadeia BBD. | O mesmo pad muda de vencedor com a cena. A03 registra perda de ataque. | Manter DigitalFractional como principal. Usar BBD internamente para pad amplo e ecos. | RETAIN / OPTIONAL |
| B · Estágios | B01–B02 | Economy no pad. Sem preferência no dedilhado curto. | 3/5 no pad. Seleção geral inconclusiva. | Intensidade ligeiramente maior com 512. Ataque curto soa semelhante. | Em M2.9 a 48 kHz ambos têm piso de 1,333 ms. Economy usa metade do teto de eventos e, no ensaio tonal, atenua mais a 10 kHz (−27,61 vs −25,85 dB). | O voto no 512 aparece numa cena. Menor carga de eventos e p99 de callback são medidas distintas. | Usar 1024 como referência técnica e repetir com 512. | RETEST |
| C · Ganho | C01–C02 | Conservative no pad baixo. Nominal nos ataques e ecos. | 4/5 e 5/5, em sentidos opostos. | Queda percebida com ambos em C01. Chiado aparente com Conservative em C02. | M2.9 mede cerca de 6 dB a mais de margem interna e 6 dB a menos de SNR pareada com Conservative. | Fonte, nível, Mix, Dynamics e feedback variam entre cenas. | Usar Nominal no protótipo e repetir a comparação cruzando nível e feedback na mesma fonte. | RETEST |
| D · Movimento | D01–D02 | Wander nos acordes. PaperNarrowband na voz intensa. | 5/5 e 4/5. Bons sinais contextuais, com fonte e Chaos diferentes. | Wander amplia a sensação de movimento. PaperNarrowband cola à voz. PhaseDrift deixa vozes mais aparentes em D02. | Mesma excursão nominal por grupo, com trajetórias nativas distintas. RMS de controle D01 Wander .722 vs Paper .609. D02 Paper .658 vs Phase .711. | A letra Z muda de algoritmo. O RMS do controle não mede integração musical nem periodicidade percebida. | Fixar Wander como referência interna e manter PaperNarrowband para confronto. | RETAIN / OPTIONAL |
| E · Banco | E01–E02 | Gentle no pad. Selective nos ataques. | 4/5 em ambos. Evidência contextual dividida. | Coesão no pad e movimento nos ataques. | Atrasos idênticos. Gentle usa três estágios de um polo em paralelo. Selective usa seis estágios em divisões residuais de dois polos. Ambos somam à entrada antes dos delays. | O movimento adicional tem benefício registrado numa cena. | Manter Gentle. Reavaliar Selective se o ganho nos ataques se repetir. | RETAIN |

### Leitura por família

**A.** A oposição entre os pads é compatível com um instrumento que cobre desde espessamento discreto até ensemble mais aberto. Como as trajetórias são iguais dentro de cada grupo, diferenças percebidas de textura, abertura, pitch e cauda devem ser investigadas na cadeia de sinal e no retorno, sem atribuição ao gerador de movimento. A04 estabelece preferência pela abertura do Digital, sem avaliação explícita de continuidade do pitch. A03 justifica estudar o primeiro ataque e a separação das repetições.

**B.** A impressão de intensidade em B01 não identifica brilho, densidade ou suavidade. No próprio pad, os RMS RAW foram .01923 para Economy e .01917 para Balanced. A resposta tonal M2.9 tem outra excitação e o clock covaria com estágios. B02 dá pouco apoio auditivo para escolher 1024 pelo ataque ou 512 pelo atraso curto. O ganho de custo do 512 precisa reaparecer nas caudas de callback em máquinas e blocos comparáveis.

**C.** C01 RAW tem RMS .002770 para ambos, cerca de 3 dB abaixo do dry correspondente após escalá-lo a 1/8. LEVEL_MATCHED iguala os dois processados entre si. A lei de Mix e o recuo de wet com Dynamics em sinal baixo são hipóteses para a impressão comum de queda. Em C02 RAW, RMS .033825 (Conservative) e .033798 (Nominal) mostram diferença global pequena. O SNR da M2.9 torna o ruído de C02 plausível e pede inspeção de silêncio, ataque e cauda. A origem do chiado e a contribuição de compander, distorção e corpo continuam em aberto.

**D.** Wander mistura seno contínuo com alvos aleatórios interpolados suavemente. PaperNarrowband modula a amplitude do portador com envelope aleatório suave. PhaseDrift varia a velocidade de fase mantendo a amplitude. A preferência por PaperNarrowband em D02 torna interessante a combinação de portador regular e amplitude variável para voz. A impressão de várias vozes com PhaseDrift vale para essa cena. Repetição artificial, naturalidade do pitch e expressividade ainda precisam de material mais longo e comparações com a mesma fonte em diferentes Chaos.

**E.** Gentle oferece sobreposição gradual em 250/1000/4000 Hz. Selective redistribui energia e fase por divisões residuais em cascata. A reconstrução do banco isolado não garante equivalência com quatro atrasos diferentes. O movimento de E02 é um ganho perceptivo contextual. O comentário não identifica melhora de punch, separação ou aumento de metalicidade. Falta evidência para trocar o padrão ou justificar mais um modo.

[Arquitetura atual](architecture.md) · [Envelope M2.9](m2_9_bbd_product_envelope.md) · [Arquitetura candidata](m3_1_musical_architecture.md).

## Validação da rodada

CTest DSP passou 24/24 e o teste de estado e roteamento do plugin passou 1/1 nos builds locais existentes. Os testes do Listening Lab passaram 4/4 com o pacote original. O verificador M3.0 conferiu 55 WAVs, com todos os hashes iguais na renderização independente. O erro máximo de RMS do casamento foi 0,000000344 dB. O pacote registra trajetórias comuns exatas, regressão Digital aprovada e zero clamps ocultos, guardas numéricas ou alocações no processamento instrumentado. Esses resultados cobrem as renderizações e as suítes executadas. A qualificação em hosts permanece na M3.1D.
