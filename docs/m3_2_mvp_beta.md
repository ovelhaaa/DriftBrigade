# M3.2 / M3.2.1 — MVP Fast Track, Visual Identity Overhaul e Friends Beta

## 1. Escopo do MVP e Decisão de Produto (DSP Freeze)

As etapas **M3.2** e **M3.2.1** marcam a transição do **DriftBrigade** da fase de pesquisa de DSP para a primeira versão candidata de produto (**Friends Beta — `v0.2.0-beta.1`**, versão numérica CMake/VST3 `0.2.0`) voltada a testes reais em produções musicais.

### Configuração Pública Congelada
- **Backend de atraso:** `DigitalFractional` (interpolação cúbica Hermite de 4 pontos com margem de segurança, slew cap de `0.25 amostra/amostra` e bloqueio DC de `5 Hz` pós-tap de feedback).
- **Modulador orgânico:** `Wander` (interpolação raised-cosine entre alvos aleatórios com fase contínua + portadora senoidal normalizada em variância).
- **Banco multibanda:** `Gentle` (quatro bandas complementares com crossovers TPT de primeira ordem em `250 Hz`, `1 kHz` e `4 kHz`, reconstrução de latência zero).
- **Parâmetros públicos:** Mantidos exatamente os 9 parâmetros (`motion`, `depth`, `center`, `chaos`, `coherence`, `dynamics`, `feedback`, `mix`, `width`), suas faixas internas, mapeamentos logarítmicos/lineares, valores iniciais, suavização de `50 ms`, esquema de estado XML (`schema=1`) e identidade VST3 (`audio.driftbrigade.m1` / `DrBg`).

Nenhum arquivo de DSP (`Source/dsp/*`) ou especificação semântica de parâmetros (`Source/params/ParameterSpecs.h`) sofreu alteração sonora ou numérica. O backend experimental `Balanced1024 BBD` e o alvo de validação `DRIFT BRIGADE BBD HOST TEST` permanecem isolados na infraestrutura interna de qualificação, fora do pacote beta distribuído aos usuários.

---

## 2. Direção de Arte e Arquitetura da Interface (M3.2.1 Overhaul)

Na **M3.2.1**, a interface foi refinada sob o conceito **Precision Instrument / Contemporary Analog Laboratory**, substituindo completamente o antigo esquema escuro ciano/âmbar por uma linguagem baseada em **Off-White + Grafite Neutro + Laranja** (com o laranja como única cor cromática de destaque):

### Paleta Definitiva (`drift::ui::Palette`)
| Elemento | Hexadecimal | Função |
|---|---|---|
| **Chassi / Background Geral** | `#F0EDE6` | Off-white levemente quente; margens externas, cabeçalho, rodapé e canais de separação entre módulos. |
| **Painéis Principais** | `#292B2D` | Grafite neutro (sem tendência azulada); estrutura do `MotionField`, alojamento dos `Quick Starts` e módulos `MOVEMENT`, `SPACE` e `CHARACTER`. |
| **Interior dos Displays** | `#17191B` | Cinza quase preto; visor rebaixado do `MotionField` e visores numéricos dos controles. |
| **Accent Principal (Único)** | `#F36B2B` | Laranja vibrante não fluorescente; curso ativo dos 9 knobs, trajetórias L/R, pontos de órbita, seletores ativos e detalhes de marca. |
| **Texto sobre Off-White** | `#292B2D` | Tipografia principal do cabeçalho e módulos de fluxo de sinal no rodapé. |
| **Texto sobre Grafite** | `#F3F0E9` | Tipografia principal nos módulos escuros, nomes dos controles e leituras numéricas. |
| **Tons Secundários e Bordas** | `#595753` / `#BAB6AD` / `#3B3E42` / `#D4CFC3` | Cinzas estritamente neutros derivados da paleta, sem qualquer segunda cor cromática. |

- **Dimensões e responsividade:** Projetada na dimensão padrão de **`960 × 600 px`**, com redimensionamento contínuo suportado entre **`760 × 520 px`** e **`1600 × 1000 px`** e suporte a fatores de escala de alta densidade (`125%`, `150%`, `200%`).

### Componentes Implementados (`Source/ui/`)
| Componente | Arquivo | Responsabilidade |
|---|---|---|
| `DriftLookAndFeel` | `Source/ui/DriftLookAndFeel.h/.cpp` | Paleta centralizada (`Palette`), tipografia, renderização vetorial dos knobs rotativos (escala de 21 divisões, marcador discreto do valor default, arco ativo laranja `#F36B2B` e corpo grafite), seletores de instrumento, tooltips e redimensionador. |
| `DriftKnob` | `Source/ui/DriftKnob.h/.cpp` | Controle rotativo individual conectado ao `APVTS` via `SliderAttachment`, formatação e leitura na camada de apresentação (`0–100%`, `Feedback 0–65%`, `Hz` e `ms` com 2 casas decimais), duplo clique para restaurar o default, ajuste fino (`Ctrl`/`Alt` + arraste) e tooltip musical. |
| `ParameterSection` | `Source/ui/ParameterSection.h/.cpp` | Três módulos em grafite `#292B2D` emoldurados pelo chassi `#F0EDE6`: **MOVEMENT** (`Motion`, `Depth`, `Chaos`), **SPACE** (`Center`, `Coherence`, `Width`) e **CHARACTER** (`Dynamics`, `Feedback`, `Mix`). |
| `MotionField` | `Source/ui/MotionField.h/.cpp` | Visualizador paramétrico central de 4 bandas estéreo e instrumento de órbita/janela de atraso com sistema monocromático laranja, separação `LIVE / HOLD` e sample rate real do host. |
| `QuickStartSelector` | `Source/ui/QuickStartSelector.h/.cpp` | Alojamento seletor de instrumento no cabeçalho para aplicar os três snapshots musicais (`SUBTLE`, `WIDE`, `DRIFT`) de forma compatível com automação do host. |
| `DriftEditor` | `Source/PluginEditor.h/.cpp` | Composição principal em chassi `#F0EDE6`, cabeçalho com emblema de 4 bandas em grafite e laranja, rodapé técnico de arquitetura de sinal e ciclo de vida RAII seguro. |

---

## 3. Comportamento do `MotionField`, `LIVE / HOLD` e Sample Rate Real

O `MotionField` é uma **visualização paramétrica honesta** do comportamento do modulador multibanda, operando sem depender de medição enganosa de espectro/LUFS e funcionando mesmo sem áudio na entrada:

1. **Sistema Monocromático Laranja e Código Gráfico L/R:**
   - Exibe as quatro bandas (`B1 LOW`, `B2 LO-MID`, `B3 HI-MID`, `B4 HIGH`) diferenciadas por posição vertical, espessura de traço (`B1` mais encorpado a `B4` mais fino), opacidade gradual e etiquetas laterais.
   - **Canal Esquerdo (`LEFT`):** Traço laranja `#F36B2B` **contínuo** com ponto frontal **sólido cheio** (`●─── LEFT`).
   - **Canal Direito (`RIGHT`):** Traço laranja `#F36B2B` **tracejado/segmentado** com opacidade diferenciada e ponto frontal em **anel vazado** (`○- - RIGHT`).
2. **Órbita Estéreo Minimalista e Janela de Atraso (`DELAY WINDOW`):**
   - Retículo circular minimalista com arco indicador de `Width`, vetores radiais sutis e os 4 nós das bandas em laranja (com anéis concêntricos discretos de persistência para `Feedback`).
   - Barra horizontal logarítmica (`0.3 – 30 ms`) com trilha neutra, intervalo previsto de excursão em laranja translúcido e cursor de alto contraste (`#F3F0E9` + `#F36B2B`) para o valor selecionado de `Center`.
3. **Política `LIVE / HOLD` Desacoplada:**
   - **LIVE:** Trajetórias animadas a 30 FPS, parâmetros e indicadores atualizados continuamente.
   - **HOLD:** Trajetórias congeladas (sem avanço de fase e sem recomputação contínua das curvas), porém o polling leve mantém os parâmetros, o readout `CENTER / EST. EXCURSION`, a `DELAY WINDOW`, `DYN / FB`, o sample rate e o destaque dos `Quick Starts` atualizados em tempo real.
4. **Excursão Paramétrica com Sample Rate Real do Host:**
   - `DriftProcessor` publica de forma lock-free (`std::atomic<double>`) a taxa de amostragem efetiva preparada em `prepareToPlay` (com fallback seguro para `48 kHz` antes da preparação).
   - O `MotionField` calcula o limite de excursão do modelo `DigitalFractional` usando `4.0 / effectiveSampleRate()` (testado em `44.1`, `48`, `88.2` e `96 kHz`) e identifica o valor explicitamente como `EST. EXCURSION`.

---

## 4. Quick Starts e Valores Documentados

Os três Quick Starts aplicam snapshots nos 9 parâmetros públicos existentes usando `beginChangeGesture()`, `setValueNotifyingHost()` e `endChangeGesture()`, preservando compatibilidade total com automação e salvamento de estado da DAW:

| Parâmetro | ID | Unidade | Default | **SUBTLE** | **WIDE** | **DRIFT** |
|---|---|---|---|---|---|---|
| **Motion** | `motion` | `Hz` | `0.70 Hz` | `0.45 Hz` | `0.85 Hz` | `0.28 Hz` |
| **Depth** | `depth` | `%` | `50 %` | `35 %` (`0.35`) | `62 %` (`0.62`) | `78 %` (`0.78`) |
| **Center** | `center` | `ms` | `8.00 ms` | `6.50 ms` | `11.50 ms` | `18.00 ms` |
| **Chaos** | `chaos` | `%` | `55 %` | `30 %` (`0.30`) | `45 %` (`0.45`) | `82 %` (`0.82`) |
| **Coherence** | `coherence` | `%` | `45 %` | `70 %` (`0.70`) | `30 %` (`0.30`) | `20 %` (`0.20`) |
| **Dynamics** | `dynamics` | `%` | `25 %` | `20 %` (`0.20`) | `25 %` (`0.25`) | `45 %` (`0.45`) |
| **Feedback** | `feedback` | `%` | `12 %` | `8 %` (`0.08`) | `18 %` (`0.18`) | `38 %` (`0.38`) |
| **Mix** | `mix` | `%` | `50 %` | `35 %` (`0.35`) | `50 %` (`0.50`) | `58 %` (`0.58`) |
| **Width** | `width` | `%` | `60 %` | `45 %` (`0.45`) | `90 %` (`0.90`) | `80 %` (`0.80`) |

---

## 5. Como Gerar a Suíte Visual e o Pacote Beta (Windows x64)

```sh
cmake -S . -B build-plugin -DDRIFT_BUILD_VST3_HOST_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin --config Release --parallel 4
ctest --test-dir build-plugin -C Release --output-on-failure
./build-plugin/drift_plugin_tests --visual-suite output/visual-suite
python tools/verify_visual_suite.py output/visual-suite
python tools/package_beta.py --build-dir build-plugin --output-dir output/beta --config Release --version 0.2.0-beta.1
```

O script `tools/package_beta.py` valida a consistência entre a versão numérica CMake (`0.2.0`) e a tag beta (`0.2.0-beta.1`), garante que o plugin interno `DRIFT BRIGADE BBD HOST TEST` não está presente e gera:
- `output/beta/DriftBrigade-0.2.0-beta.1-win-x64/` (diretório completo)
- `output/beta/DriftBrigade-0.2.0-beta.1-win-x64.zip` (pacote pronto para compartilhamento contendo `VST3/DRIFT BRIGADE.vst3`, `Standalone/DRIFT BRIGADE.exe`, `README_INSTALACAO.md`, `README_USO.md`, `BETA_TEST_GUIDE.md`, `CHANGELOG.md` e `VERSION.txt`).
