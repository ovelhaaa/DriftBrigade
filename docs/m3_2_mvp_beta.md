# M3.2 — MVP Fast Track: DSP Freeze, Interface Motion Field e Friends Beta

## 1. Escopo do MVP e Decisão de Produto (DSP Freeze)

A etapa **M3.2** marca a transição do **DriftBrigade** da fase de pesquisa de DSP para a primeira versão candidata de produto (**Friends Beta — `v0.2.0-beta.1`**) voltada a testes reais em produções musicais.

### Configuração Pública Congelada
- **Backend de atraso:** `DigitalFractional` (interpolação cúbica Hermite de 4 pontos com margem de segurança, slew cap de `0.25 amostra/amostra` e bloqueio DC de `5 Hz` pós-tap de feedback).
- **Modulador orgânico:** `Wander` (interpolação raised-cosine entre alvos aleatórios com fase contínua + portadora senoidal normalizada em variância).
- **Banco multibanda:** `Gentle` (quatro bandas complementares com crossovers TPT de primeira ordem em `250 Hz`, `1 kHz` e `4 kHz`, reconstrução de latência zero).
- **Parâmetros públicos:** Mantidos exatamente os 9 parâmetros (`motion`, `depth`, `center`, `chaos`, `coherence`, `dynamics`, `feedback`, `mix`, `width`), suas faixas, mapeamentos logarítmicos/lineares, valores iniciais, suavização de `50 ms`, esquema de estado XML (`schema=1`) e identidade VST3 (`audio.driftbrigade.m1` / `DrBg`).

Nenhum arquivo de DSP (`Source/dsp/*`), especificação de parâmetros (`Source/params/*`) ou processamento de áudio (`PluginProcessor.cpp`) sofreu alteração sonora ou numérica. O backend experimental `Balanced1024 BBD` e o alvo de validação `DRIFT BRIGADE BBD HOST TEST` permanecem isolados na infraestrutura interna de qualificação, fora do pacote beta distribuído aos usuários.

---

## 2. Direção de Arte e Arquitetura da Interface

A interface foi reprojetada do zero em C++17/JUCE vetorial (`Source/ui/`) seguindo a direção estética **Industrial Contemporâneo + Tecnologia Musical + Instrumentação Digital Sofisticada**:

- **Paleta cromática:** Chassi em grafite profundo (`#0C1016`–`#111720`), painéis instrumentais em ardósia escura (`#151C26`), viewport rebaixado (`#090D12`), acentos principais em **ciano mineral** (`#38D4C8`) para coerência/canal esquerdo e **âmbar suave** (`#E5A44C`) para caráter/canal direito/marcadores de referência.
- **Dimensões e responsividade:** Projetada na dimensão padrão de **`960 × 600 px`**, com redimensionamento contínuo suportado entre **`760 × 520 px`** e **`1600 × 1000 px`** e suporte a fatores de escala de alta densidade (`125%`, `150%`, `200%`).

### Componentes Implementados (`Source/ui/`)
| Componente | Arquivo | Responsabilidade |
|---|---|---|
| `DriftLookAndFeel` | `Source/ui/DriftLookAndFeel.h/.cpp` | Paleta técnica, tipografia, renderização vetorial dos knobs rotativos (escala de 21 divisões, marcador âmbar do valor default, arco ativo e corpo usinado), botões, tooltips e redimensionador. |
| `DriftKnob` | `Source/ui/DriftKnob.h/.cpp` | Controle rotativo individual conectado ao `APVTS` via `SliderAttachment`, título, caixa numérica editável com unidade (`Hz`, `ms`, `%`), duplo clique para restaurar o default, ajuste fino (`Ctrl`/`Alt` + arraste) e tooltip musical. |
| `ParameterSection` | `Source/ui/ParameterSection.h/.cpp` | Agrupamento visual em três baias temáticas: **MOVEMENT** (`Motion`, `Depth`, `Chaos`), **SPACE** (`Center`, `Coherence`, `Width`) e **CHARACTER** (`Dynamics`, `Feedback`, `Mix`). |
| `MotionField` | `Source/ui/MotionField.h/.cpp` | Visualizador paramétrico central de 4 bandas estéreo e escopo de órbita/janela de atraso a 30 FPS. |
| `QuickStartSelector` | `Source/ui/QuickStartSelector.h/.cpp` | Seletor superior para aplicar os três snapshots musicais (`SUBTLE`, `WIDE`, `DRIFT`) de forma compatível com automação do host. |
| `DriftEditor` | `Source/PluginEditor.h/.cpp` | Composição principal, cabeçalho com emblema de 4 bandas, rodapé técnico de fluxo de sinal e ciclo de vida RAII seguro. |

---

## 3. Comportamento do `MotionField` e Honestidade Visual

O `MotionField` é uma **visualização paramétrica honesta** do comportamento do modulador multibanda, operando sem depender de medição enganosa de espectro/LUFS e funcionando mesmo sem áudio na entrada:

1. **Painel Principal (Esquerda — Trajetórias Estéreo de 4 Bandas):**
   - Exibe as quatro bandas (`B1 LOW`, `B2 LO-MID`, `B3 HI-MID`, `B4 HIGH`) nos canais esquerdo (**ciano**) e direito (**âmbar**) usando a mesma lei matemática de interpolação `Wander`, taxas por banda (`0.91x, 1.037x, 1.083x, 0.967x`) e taxas estéreo (`1.113x, 0.943x, 1.057x, 0.887x`) do motor DSP.
   - **Motion:** controla a velocidade de propagação temporal e a densidade de ciclos na janela.
   - **Depth:** controla a amplitude vertical de excursão e atualiza o readout real de excursão em milissegundos (`EXCURSION +/- ms`) calculado por `depthSeconds(motion, depth)`.
   - **Chaos:** transforma continuamente a forma das curvas entre senoides regulares (`0%`) e deriva orgânica `Wander` (`100%`).
   - **Coherence:** aproxima ou separa o movimento e o espaçamento vertical entre as quatro bandas.
   - **Width:** separa as trajetórias esquerda (ciano) e direita (âmbar) e preenche a fita estéreo intermediária.
   - **Center:** desloca a referência temporal na barra logarítmica (`0.3 – 30 ms`).
   - **Dynamics:** exibe o corredor paramétrico de sensibilidade dinâmica (`DYN %`).
   - **Feedback:** projeta trilhas fantasma de eco recursivo e halos de ressonância (`FB %`).
   - **Mix:** controla a opacidade entre o eixo de referência seco tracejado (`DRY REFERENCE` em `Mix = 0%`) e as trajetórias processadas.
2. **Escopo de Órbita Estéreo e Janela de Atraso (Direita):**
   - Mostra em 2D o posicionamento instantâneo `L/R` vs. deslocamento de atraso das 4 bandas e uma barra horizontal logarítmica (`0.3 – 30 ms`) com o marcador âmbar de `Center` e a faixa ciano de excursão ativa.
3. **Thread-Safety e Desempenho:**
   - Leitura de parâmetros exclusivamente via `ParameterReader` (`std::atomic<float>` lock-free).
   - Zero chamadas gráficas ou alocações no callback de áudio.
   - Buffer de coordenadas 100% pré-alocado (`std::array`), atualização limitada a `30 FPS` apenas na região do `MotionField`, pausa automática quando o editor está oculto (`!isShowing()`) e botão **LIVE / HOLD** para congelar a animação.

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

## 5. Validação Funcional e Visual Realizada

1. **Testes de Regressão DSP e Qualificação:** Todas as suítes (`dsp_qualification`, `bbd_listening_numerical`, `bbd_engine_numerical`, `bbd_product_admission`, `bbd_host_lifecycle`, etc.) continuam passando sem alteração numérica.
2. **Testes do Plugin e Host VST3 (`drift_plugin_tests` e `drift_vst3_host_tests`):**
   - Verificação dos 9 knobs, unidades, tooltips e retorno ao default por duplo clique.
   - Aplicação dos 3 Quick Starts, sincronização com os knobs, persistência via `getStateInformation`/`setStateInformation` e desmarcação automática ao editar manualmente qualquer controle.
   - Resposta determinística e zero alocações no `MotionField::stepSimulation()` e no callback `processBlock()`.
   - Abertura e fechamento repetidos do editor, renderização offline com automação simultânea nos 9 parâmetros, roteamento mono/estéreo/upmix e bypass VST3.
3. **Inspeção Visual Automatizada (`--visual-suite`):**
   - Capturas geradas a partir do executável real em `960×600` (padrão), `760×520` (mínimo), `1280×800` (ampliado), escalas DPI `125%`, `150%` e `200%`, controles em valores mínimos (`Mix 0%`), valores máximos e nos três Quick Starts (`SUBTLE`, `WIDE`, `DRIFT`).

---

## 6. Como Gerar o Pacote Beta (Windows x64)

```sh
cmake -S . -B build-plugin -DDRIFT_BUILD_VST3_HOST_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin --config Release --parallel 4
ctest --test-dir build-plugin -C Release --output-on-failure
python tools/package_beta.py --build-dir build-plugin --output-dir output/beta --config Release --version 0.2.0-beta.1
```

O script `tools/package_beta.py` valida que o plugin interno `DRIFT BRIGADE BBD HOST TEST` não está presente no diretório de artefatos e gera:
- `output/beta/DriftBrigade-0.2.0-beta.1-win-x64/` (diretório completo)
- `output/beta/DriftBrigade-0.2.0-beta.1-win-x64.zip` (pacote pronto para compartilhamento contendo `VST3/DRIFT BRIGADE.vst3`, `Standalone/DRIFT BRIGADE.exe`, `README_INSTALACAO.md`, `README_USO.md`, `BETA_TEST_GUIDE.md`, `CHANGELOG.md` e `VERSION.txt`).

---

## 7. Limites Conhecidos e O Que Ficou Para Depois

- **Assinatura Digital:** Os binários Windows desta beta privada não possuem certificado Authenticode comercial; essa condição está documentada no `README_INSTALACAO.md` sem recomendar desativar proteções do sistema.
- **Atribuição JUCE:** Como o build utiliza a configuração padrão JUCE 7 sem licença comercial ativa no CMake, o selo "Made with JUCE" aparece no canto inferior direito; o layout reserva uma barra técnica inferior de `60 px` para garantir que nenhum controle ou valor numérico seja obstruído.
- **Fora de escopo nesta versão (planejado para etapas futuras):** Sistema completo de gerenciamento/navegação de presets em arquivo, MIDI learn, medição de nível de áudio de entrada/saída na GUI, instalador `.msi`/`.exe` assinado e decisão final sobre exposição pública do modo BBD.
