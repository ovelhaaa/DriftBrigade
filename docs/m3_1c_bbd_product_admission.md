# M3.1C — Admissão interna do BBD

## Base e correção do CI

A base de trabalho é `915738239c57694b5c34cf350c3998fcfb27ee7b` (M3.1B.1). Ela já contém `98767960ad91c2173d7b7a503d33aa3e8c594a72` (M3.0 e head do PR #11), descendente de `dedf4cb76e15a335159cb4c227f66b9ae69224bb` (main remoto, M2.9). A main local estava em `03a5625`. As decisões M3.1A constam da documentação da M3.1B.1. A branch M3.1C parte diretamente dessa cadeia.

O [job Linux original](https://github.com/ovelhaaa/DriftBrigade/actions/runs/43110586660/job/130825232773) executou Release em Ubuntu 26.04 com GCC 15.2. CTest saiu com código 8. `bbd_listening_numerical` e `bbd_listening_voice_numerical` falharam em `Frozen Digital regression failed`, `bbd_engine_numerical` em `digital frozen engine bit identity` e `bbd_performance_numerical` em `core event telemetry differs`. Os testes de stream safety, inclusive o do reteste vocal, passaram.

O GCC 14.2 local com `-mfma` reproduziu as três falhas de identidade Digital. O commit `9fe7b22` desativa a contração implícita de ponto flutuante nos alvos C++ do projeto para GCC e Clang, incluindo referências congeladas e DSP inline. As quatro verificações passaram com `-mfma -ffp-contract=off` localmente e o [job Ubuntu corrigido](https://github.com/ovelhaaa/DriftBrigade/actions/runs/43144595114/job/130939329059) passou 26/26. A comparação continua bit a bit e as fixtures de audição continuam usando as mesmas entradas e configurações.

## Arquitetura e perfil implementado

O plugin chama o `prepare` legado com DigitalFractional. O seletor interno `setDelayBackend` continua configurando a preparação das fixtures de pesquisa. A preparação compartilhada inicializa os suavizadores, dois bancos, oito vozes, moduladores e seguidores de envelope. Cada voz BBD recebe seu perfil físico e uma seed derivada da seed da sessão e da banda. O core calcula o clock a partir do delay efetivo e avança capturas e saídas alternadas. O caminho de pesquisa limita Center e Depth no motor e usa o teto experimental de 128 eventos por amostra no core.

`DriftEngine::prepareProduct` opta explicitamente pelo perfil `Balanced1024` a cada preparação. A configuração canônica reutiliza a fixture de 1024 estágios físicos, ganho Nominal, Holters/Parker Table1, compander de 0,47 µF e 10 kΩ, caráter M2.2, transferência EngineeringPolynomial de força 1, ExternalWetReturn e bloqueio DC de saída de 5 Hz. O motor fixa Wander, Gentle, profundidade perceptual e a resposta Dynamics atual durante a sessão BBD. Os bancos, controles e mistura são compartilhados com DigitalFractional.

| Taxa (Hz) | Teto (Hz) | Piso (ms) | Situação de Balanced1024 |
| ---: | ---: | ---: | --- |
| 44.100 | 352.800 | 1,451247 | Elegível |
| 48.000 | 384.000 | 1,333333 | Elegível |
| 88.200 | 384.000 | 1,333333 | Elegível |
| 96.000 | — | — | `UnsupportedSampleRate` |
| 176.400 | — | — | `UnsupportedSampleRate` |
| 192.000 | — | — | `UnsupportedSampleRate` |

O teto é `min(384000, 8 * sampleRate, clockCeilingHz)` e o piso é `1024 / (2 * teto)` segundos. Um teto solicitado menor pode restringir a sessão. O core recebe o teto na preparação de cada voz, aplica o piso ao delay e limita o clock também após a divisão em ponto flutuante. A fase do scheduler pode carregar uma fração pendente. O orçamento de eventos inteiros é `ceil(2 * teto / sampleRate)`, sempre até 16, inclusive quando a soma de fases arredonda para o inteiro seguinte.

## Center, Depth e automação

O pedido inclui valores iniciais e uma caixa fechada com mínimo e máximo para cada controle. Uma caixa degenerada declara uma cena fixa. A cena pública inicial com Center de 8 ms não cabe na reserva conservadora de Balanced1024 para o Depth e Motion padrão. Para uma cena interna, por exemplo, Center de 12 ms e os demais valores padrão são elegíveis nas três taxas.

O menor Center da caixa deve alcançar o piso. O maior excursionamento solicitado precisa caber na menor folga, considerando o menor Center até o piso e o maior Center até o teto de delay de 55 ms. A folga é multiplicada por 0,85 e dividida pelo limite de modulação Wander. Os 15% são uma escolha de engenharia já usada pelo motor, não um limite físico medido.

Para um Depth fixo, a curva é afim em Motion abaixo de 4 Hz e não crescente a partir de 4 Hz. Acima de 4 Hz os dois níveis da curva caem com Motion, a interpolação geométrica preserva essa ordem e a curva estabiliza a partir de 9 Hz. A curva cresce com Depth em toda a faixa. Portanto o máximo na caixa está no Depth máximo e no maior dos valores de excursão calculados no Motion mínimo e no ponto de 4 Hz limitado ao intervalo de Motion.

O limite de Wander é √3 para todo Chaos efetivo, inclusive a contribuição de Dynamics. Coherence e Width usam dois pares de pesos de norma 1. Cada soma tem norma L1 de até √2, então o limite composto é 2√3. A validação usa todos os extremos independentes da caixa. O suavizador de cada controle é uma combinação convexa entre o estado corrente e o novo alvo. A limitação de velocidade do delay caminha do delay corrente em direção ao próximo alvo dentro da faixa. A combinação vale também para mudanças simultâneas e automação com transições sobrepostas.

`setParameters` valida cada atualização inteira antes de alterar os alvos. A sessão admitida aplica a excursão pedida, com guardas físicas no limite do motor e do core. Atualizações inválidas ou fora da caixa deixam os últimos alvos válidos ativos e reportam `InvalidParameters` ou `OutsideEnvelope` em `bbdAutomationStatus()`. A leitura desse status deve ocorrer na thread de áudio ou com processamento parado.

## Decisões e ciclo de vida

`evaluateBBDAdmission` é determinística e independente do estado do motor. O resultado registra perfil, configuração, valores iniciais, caixa declarada, taxa solicitada, limites calculados, backend preparado e sua taxa. O status preserva o motivo BBD e o resultado de preparação identifica BBD, fallback Digital, indisponibilidade ou falha do fallback.

- `InvalidConfiguration` cobre taxa inválida, contagem física inválida, teto inválido, controles não finitos ou fora das especificações e parâmetros iniciais fora da caixa.
- `UnsupportedProfile` cobre perfil, contagem válida ou ganho diferentes de Balanced1024. `UnsupportedSampleRate` identifica taxas válidas fora da lista.
- `CenterBelowFloor`, `ExcursionOutOfRange` e `ClockLimitExceeded` identificam violações do envelope.
- `PreparationFailed` identifica falha ao inicializar o BBD após a admissão. `FallbackToDigital` confirma que o Digital foi preparado. `DigitalPreparationFailed` identifica a falha da tentativa Digital e preserva o status BBD.

A preparação e o reset são serializados pelo chamador com o processamento parado. O motor publica prontidão depois de preparar e reinicializar as oito vozes. Em `prepareProduct`, BBD e Digital usam a taxa exata solicitada pelo host. O fallback Digital aceita taxas finitas de 44.100 a 192.000 Hz, inclusive. Sem backend compatível, o resultado é `Unavailable`. Taxas não finitas ou fora do domínio de entrada de 8 a 384 kHz recebem `InvalidConfiguration`. A normalização de taxa continua restrita ao `prepare` legado.

O BBD aplica os valores iniciais validados na admissão. O fallback Digital limita cada valor inicial finito ao intervalo de `parameterSpecs` e substitui NaN e infinitos pelo valor inicial da especificação daquele controle. A caixa de automação se aplica à sessão BBD. `request.initial` registra o pedido original e `appliedInitial` registra os alvos usados no reset da preparação bem-sucedida.

Em caso de indisponibilidade ou falha parcial, `preparedBackend` é `Unavailable`, `preparedSampleRate` é zero e `appliedInitial` fica vazio. O motor devolve silêncio até uma nova preparação bem-sucedida. Os buffers já alocados podem ser reaproveitados nessa preparação.

O reset limpa bancos, suavizadores, moduladores, armazenamento BBD, filtros, compander, retorno e bloqueador DC. O BBD admitido volta ao último Center válido e restaura as seeds. Reprepare descarta a cauda e reavalia a sessão. O tamanho de bloco não faz parte da configuração das vozes. Não há troca de backend no callback, novas alocações, locks, I/O, parâmetros APVTS ou alteração da topologia de feedback.

## Qualificação e M3.1D

O CTest `bbd_product_admission` cobre a matriz de taxas, limites exatos, configurações inválidas, arredondamento de fase no teto, automação combinada, rejeição atômica de controles, oito vozes, guardas e clamps, alocações, roteamento mono, transientes, cauda, reset e reprepare. Compara o fallback à saída Digital de referência e injeta falhas após quatro vozes de cada backend. A injeção existe somente no executável de qualificação. Os gates existentes cobrem M2.9, M3.0, M3.1B.1, regressão Digital congelada e o contrato do plugin.

O próximo marco é validar a integração em hosts, incluindo ciclos de ativação, tamanhos de bloco variáveis, mono/estéreo, restauração de sessão, automação do host e custo por callback nos sistemas-alvo. A M2.9 fornece medições condicionadas às cenas e máquinas qualificadas. A M3.1C mantém o perfil restrito a sessões internas declaradas. Uma eventual troca audível de backend requer projeto próprio de fase, cauda e crossfade.
