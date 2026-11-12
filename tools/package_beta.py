#!/usr/bin/env python3
"""Package the DriftBrigade M3.2 Windows x64 Friends Beta distribution ZIP."""

from __future__ import annotations

import argparse
import datetime
import pathlib
import shutil
import subprocess
import sys
import zipfile


def git_rev_parse(repo_root: pathlib.Path, short: bool = False) -> str:
    cmd = ["git", "rev-parse"]
    if short:
        cmd.append("--short=8")
    cmd.append("HEAD")
    try:
        out = subprocess.check_output(cmd, cwd=repo_root, text=True).strip()
        return out
    except Exception:
        return "unknown"


def write_text(path: pathlib.Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def build_install_readme(version_tag: str, commit_sha: str) -> str:
    return f"""# DRIFT BRIGADE — Guia de Instalacao (Friends Beta)

Versao: `{version_tag}`
Commit: `{commit_sha}`
Plataforma: Windows x64 (VST3 + Standalone)

> **AVISO DE VERSAO BETA:** Esta e uma versao beta privada destinada a testes musicais e avaliacao de usabilidade. Os executaveis desta distribuicao de testes nao possuem assinatura digital comercial (Authenticode); o Windows SmartScreen pode exibir um aviso informativo na primeira execucao do aplicativo Standalone. Verifique sempre a origem do pacote antes de executar.

---

## Conteudo do Pacote

- `VST3/DRIFT BRIGADE.vst3/` — Bundle oficial do plugin VST3 (64-bit) para DAWs no Windows.
- `Standalone/DRIFT BRIGADE.exe` — Aplicativo independente para testar rapidamente sem abrir uma DAW.
- `README_USO.md` — Guia rapido dos controles, Motion Field e Quick Starts.
- `BETA_TEST_GUIDE.md` — Roteiro de avaliacao musical e modelo para reportar problemas.
- `CHANGELOG.md` — Historico de mudancas desta versao beta.
- `VERSION.txt` — Identificacao tecnica da build e commit.

---

## Como Instalar o Plugin VST3

1. Extraia todo o conteudo do arquivo `.zip` em uma pasta temporaria.
2. Copie a pasta completa **`DRIFT BRIGADE.vst3`** (localizada dentro de `VST3/`) para o diretorio padrao de plugins VST3 do Windows:
   ```
   C:\\Program Files\\Common Files\\VST3\\
   ```
3. Abra sua DAW (REAPER, Ableton Live, Cubase, Studio One, FL Studio, Bitwig, etc.) e execute o re-scan de plugins VST3 (se necessario).
4. Procure por **DRIFT BRIGADE** (fabricante: **Drift Audio**).

### Atualizacao de Versoes de Desenvolvimento
O identificador do plugin VST3 (`audio.driftbrigade.m1` / `DrBg`) e o formato de estado (`schema=1`) foram preservados. Voce pode substituir o bundle `DRIFT BRIGADE.vst3` existente sem perder os ajustes salvos em projetos anteriores.

---

## Como Usar o Aplicativo Standalone

1. Execute `Standalone/DRIFT BRIGADE.exe`.
2. Clique em **Options -> Audio/MIDI Settings...** no topo da janela para selecionar sua interface de audio (ASIO / WASAPI), entradas, saidas, taxa de amostragem e tamanho de buffer.
"""


def build_usage_readme(version_tag: str) -> str:
    return f"""# DRIFT BRIGADE — Guia Rapido de Utilizacao ({version_tag})

**DRIFT BRIGADE** e um efeito de modulacao e atraso organico multibanda (4 bandas complementares) projetado para criar movimento estereo, profundidade de chorus e texturas vivas sem desfigurar o contorno da fonte sonora.

---

## Pontos de Partida: QUICK STARTS

No topo direito da interface, escolha um dos tres pontos de partida musicais:

- **SUBTLE:** Movimento discreto e coeso, preservando a definicao da fonte com abertura estereo natural. Ideal para violoes, vozes, Rhodes e synths principais.
- **WIDE:** Chorus aberto e expressivo, explorando desacoplamento entre bandas (`Coherence 30%`) e largura estereo (`Width 90%`). Ideal para guitarras limpas, teclados e camadas estereo.
- **DRIFT:** Movimento organico profundo e irregular (`Chaos 82%`, `Center 18 ms`, `Feedback 38%`). Ideal para pads, cordas, texturas e ambiencias.

Apos clicar em qualquer Quick Start, todos os nove controles continuam totalmente livres para edicao e automacao.

---

## Visualizador Central: MOTION FIELD

O **Motion Field** apresenta em tempo real a representacao parametrica das **quatro bandas de frequencia** (`B1 LOW`, `B2 LO-MID`, `B3 HI-MID`, `B4 HIGH`) nos canais esquerdo (**ciano**) e direito (**ambar**):

- **Painel Principal (Esquerda):** Trajetorias de modulacao no tempo, refletindo velocidade (`Motion`), amplitude (`Depth`), irregularidade (`Chaos`), acoplamento entre bandas (`Coherence`), abertura L/R (`Width`), sensibilidade dinamica (`Dynamics`), caudas de realimentacao (`Feedback`) e balanco seco/processado (`Mix`).
- **Escopo Estereo e Janela de Atraso (Direita):** Orbita estereo das quatro bandas e barra logaritmica (`0.3 - 30 ms`) indicando a posicao de `Center` e a excursao ativa.
- **Botao LIVE / HOLD:** Permite pausar a animacao (30 FPS) a qualquer momento.
- *Nota de honestidade visual:* O Motion Field e uma visualizacao fiel do comportamento dos parametros configurados, funcionando mesmo sem sinal de audio na entrada.

---

## Os Nove Controles Musicais

### 1. MOVEMENT
- **Motion (`0.05 - 10.00 Hz`):** Velocidade base das trajetorias de modulacao.
- **Depth (`0 - 100 %`):** Amplitude do deslocamento de tempo ajustada perceptualmente pela taxa.
- **Chaos (`0 - 100 %`):** Transicao continua entre oscilacao senoidal pura (`0%`) e deriva aleatoria suave Wander (`100%`).

### 2. SPACE
- **Center (`0.30 - 30.00 ms`):** Atraso base das vozes — valores curtos (`0.3 - 3 ms`) produzem efeito de flanger/comb; valores medios (`5 - 15 ms`) produzem chorus classico; valores longos (`15 - 30 ms`) produzem doubling e ambiencia.
- **Coherence (`0 - 100 %`):** Em `100%`, as quatro bandas movem-se juntas; em `0%`, cada banda segue uma trajetoria independente.
- **Width (`0 - 100 %`):** Decorrelacao estereo entre os canais esquerdo e direito.

### 3. CHARACTER
- **Dynamics (`0 - 100 %`):** Sensibilidade ao envelope do sinal de entrada (aumenta sutilmente o movimento e feedback em passagens fortes e atenua a cauda processada em passagens silenciosas).
- **Feedback (`0 - 65 %`):** Realimentacao positiva das linhas de atraso para maior riqueza harmonica e sustentacao.
- **Mix (`0 - 100 %`):** Balanco entre o sinal seco original (`0%`) e o sinal processado (`100%`).

### Dicas de Interacao
- **Arraste vertical/horizontal:** Ajusta o valor do knob.
- **Ctrl / Alt / Command + Arraste:** Ajuste fino de precisao.
- **Duplo clique no knob:** Restaura imediatamente o valor padrao de fabrica.
- **Clique na caixa numerica:** Permite digitar o valor exato pelo teclado.
"""


def build_changelog(version_tag: str, commit_sha: str) -> str:
    today = datetime.date.today().isoformat()
    return f"""# Changelog — DRIFT BRIGADE

## [{version_tag}] — {today} (Friends Beta MVP — M3.2)

Commit de referencia: `{commit_sha}`

### Adicionado
- **Nova Interface Profissional (960x600 redimensionavel):** Identidade visual original em grafite escuro, ciano mineral e ambar suave (`DriftLookAndFeel`, `DriftKnob`, `ParameterSection`).
- **Visualizador Parametrico `MotionField` (30 FPS):** Representacao em tempo real das quatro trajetorias multibanda e orbita estereo L/R respondendo aos 9 parametros, com modo `LIVE / HOLD` e zero trabalho no callback de audio.
- **Tres Quick Starts (`SUBTLE`, `WIDE`, `DRIFT`):** Pontos de partida musicais integrados ao APVTS e automacao do host.
- **Interacoes Refinadas nos Controles:** Indicador do valor default no anel de cada knob, duplo clique para reset, ajuste fino com tecla modificadora, edicao numerica direta e tooltips explicativos em portugues.
- **Pacote de Distribuicao Friends Beta (Windows x64):** Bundle VST3 + Standalone + documentacao de instalacao, uso e roteiro de feedback.

### Congelado / Preservado (DSP Freeze)
- Motor de audio publico congelado em **DigitalFractional + Wander + Gentle (4 bandas)** sem nenhuma alteracao sonora ou numerica em relacao a M3.1D.
- Mantidos inalterados os 9 IDs de parametros, faixas, valores iniciais, suavizacao de 50 ms, serializacao XML (`schema=1`) e identidade VST3 (`audio.driftbrigade.m1` / `DrBg`).
- Backend experimental `Balanced1024 BBD` mantido estritamente isolado na infraestrutura interna de qualificacao.
"""


def main() -> int:
    parser = argparse.ArgumentParser(description="Package DriftBrigade Windows x64 Friends Beta.")
    parser.add_argument("--build-dir", required=True, type=pathlib.Path,
                        help="CMake build directory containing DriftBrigade_artefacts.")
    parser.add_argument("--output-dir", required=True, type=pathlib.Path,
                        help="Output directory for staging and final ZIP archive.")
    parser.add_argument("--config", default="Release",
                        help="Build configuration (default: Release).")
    parser.add_argument("--version", default="0.2.0-beta.1",
                        help="Beta version string.")
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parent.parent
    artefacts_root = args.build_dir / "DriftBrigade_artefacts" / args.config
    if not artefacts_root.exists():
        # Single-config generators may place artefacts directly or under Release
        alt_root = args.build_dir / "DriftBrigade_artefacts"
        if alt_root.exists():
            artefacts_root = alt_root
        else:
            raise FileNotFoundError(f"Artefacts directory not found: {artefacts_root}")

    vst3_bundle = artefacts_root / "VST3" / "DRIFT BRIGADE.vst3"
    standalone_exe = artefacts_root / "Standalone" / "DRIFT BRIGADE.exe"

    # Guard against accidentally packaging the internal BBD host test target
    forbidden_bbd = artefacts_root / "VST3" / "DRIFT BRIGADE BBD HOST TEST.vst3"
    if forbidden_bbd.exists():
        raise RuntimeError(
            "Refusing to package beta from a directory containing DRIFT BRIGADE BBD HOST TEST.vst3"
        )
    if not vst3_bundle.exists():
        raise FileNotFoundError(f"Public VST3 bundle not found at: {vst3_bundle}")
    if not standalone_exe.exists():
        raise FileNotFoundError(f"Public Standalone executable not found at: {standalone_exe}")

    full_sha = git_rev_parse(repo_root, short=False)
    short_sha = git_rev_parse(repo_root, short=True)
    package_Folder_name = f"DriftBrigade-{args.version}-win-x64"

    args.output_dir.mkdir(parents=True, exist_ok=True)
    staging_dir = args.output_dir / package_Folder_name
    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    staging_dir.mkdir(parents=True, exist_ok=True)

    # 1. Copy VST3 bundle
    shutil.copytree(vst3_bundle, staging_dir / "VST3" / "DRIFT BRIGADE.vst3")

    # 2. Copy Standalone executable
    (staging_dir / "Standalone").mkdir(parents=True, exist_ok=True)
    shutil.copy2(standalone_exe, staging_dir / "Standalone" / "DRIFT BRIGADE.exe")

    # 3. Generate READMEs, CHANGELOG, VERSION.txt, and copy BETA_TEST_GUIDE.md
    write_text(staging_dir / "README_INSTALACAO.md", build_install_readme(args.version, full_sha))
    write_text(staging_dir / "README_USO.md", build_usage_readme(args.version))
    write_text(staging_dir / "CHANGELOG.md", build_changelog(args.version, full_sha))

    beta_guide_src = repo_root / "docs" / "mvp_beta_test_guide.md"
    if beta_guide_src.exists():
        shutil.copy2(beta_guide_src, staging_dir / "BETA_TEST_GUIDE.md")

    now_utc = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    version_info = (
        f"Product: DRIFT BRIGADE (Friends Beta)\n"
        f"Version: {args.version}\n"
        f"Commit: {full_sha}\n"
        f"ShortCommit: {short_sha}\n"
        f"BuildConfig: {args.config}\n"
        f"Platform: Windows-x64\n"
        f"DSPBackend: DigitalFractional / Wander / Gentle (Frozen)\n"
        f"VST3BundleID: audio.driftbrigade.m1\n"
        f"VST3PluginCode: DrBg\n"
        f"PackagedAtUTC: {now_utc}\n"
    )
    write_text(staging_dir / "VERSION.txt", version_info)

    # 4. Create ZIP archive
    zip_path = args.output_dir / f"{package_Folder_name}.zip"
    if zip_path.exists():
        zip_path.unlink()

    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for file_path in sorted(staging_dir.rglob("*")):
            if file_path.is_file():
                arcname = file_path.relative_to(args.output_dir)
                zf.write(file_path, arcname)

    print(f"Packaged beta directory: {staging_dir}")
    print(f"Packaged beta archive:   {zip_path} ({zip_path.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
