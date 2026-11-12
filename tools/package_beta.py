#!/usr/bin/env python3
"""Package the DriftBrigade M3.2.1 Windows x64 Friends Beta distribution ZIP."""

from __future__ import annotations

import argparse
import datetime
import pathlib
import re
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


def read_cmake_versions(repo_root: pathlib.Path) -> tuple[str, str]:
    cmake_text = (repo_root / "CMakeLists.txt").read_text(encoding="utf-8")
    proj_match = re.search(r"project\s*\(\s*DriftBrigade\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", cmake_text)
    if not proj_match:
        raise RuntimeError("Could not parse project(DriftBrigade VERSION X.Y.Z) from CMakeLists.txt")
    numeric_version = proj_match.group(1)

    suffix_match = re.search(r'set\s*\(\s*DRIFT_RELEASE_SUFFIX\s+"([^"]*)"', cmake_text)
    suffix = suffix_match.group(1) if suffix_match else "-beta.1"
    return numeric_version, f"{numeric_version}{suffix}"


def write_text(path: pathlib.Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")


def build_install_readme(version_tag: str, numeric_version: str, commit_sha: str) -> str:
    return f"""# DRIFT BRIGADE — Guia de Instalacao (Friends Beta)

Versao do Pacote: `{version_tag}` (VST3 / CMake Version: `{numeric_version}`)
Commit: `{commit_sha}`
Plataforma: Windows x64 (VST3 + Standalone)

> **AVISO DE VERSAO BETA:** Esta e uma versao beta privada destinada a testes musicais e avaliacao de usabilidade. Os executaveis desta distribuicao de testes nao possuem assinatura digital comercial (Authenticode); o Windows SmartScreen pode exibir um aviso informativo na primeira execucao do aplicativo Standalone. Verifique sempre a origem do pacote antes de executar.

---

## Conteudo do Pacote

- `VST3/DRIFT BRIGADE.vst3/` — Bundle oficial do plugin VST3 (64-bit, v`{numeric_version}`) para DAWs no Windows.
- `Standalone/DRIFT BRIGADE.exe` — Aplicativo independente para testar rapidamente sem abrir uma DAW.
- `README_USO.md` — Guia rapido dos controles, Motion Field e Quick Starts.
- `BETA_TEST_GUIDE.md` — Roteiro de avaliacao musical e modelo para reportar problemas.
- `CHANGELOG.md` — Historico de mudancas desta versao beta.
- `VERSION.txt` — Identificacao tecnica da build, versao e commit.

---

## Como Instalar o Plugin VST3

1. Extraia todo o conteudo do arquivo `.zip` em uma pasta temporaria.
2. Copie a pasta completa **`DRIFT BRIGADE.vst3`** (localizada dentro de `VST3/`) para o diretorio padrao de plugins VST3 do Windows:
   ```
   C:\\Program Files\\Common Files\\VST3\\
   ```
3. Abra sua DAW (REAPER, Ableton Live, Cubase, Studio One, FL Studio, Bitwig, etc.) e execute o re-scan de plugins VST3 (se necessario).
4. Procure por **DRIFT BRIGADE** (fabricante: **Drift Audio**, versao reportada ao host: `{numeric_version}`).

### Compatibilidade de Projetos e Estado
O identificador do plugin VST3 (`audio.driftbrigade.m1` / `DrBg`) e o formato de estado (`schema=1`) foram integralmente preservados. Voce pode substituir o bundle `DRIFT BRIGADE.vst3` existente sem perder os ajustes e automacoes salvos em projetos anteriores.

---

## Como Usar o Aplicativo Standalone

1. Execute `Standalone/DRIFT BRIGADE.exe`.
2. Clique em **Options -> Audio/MIDI Settings...** no topo da janela para selecionar sua interface de audio (ASIO / WASAPI), entradas, saidas, taxa de amostragem e tamanho de buffer.
"""


def build_usage_readme(version_tag: str) -> str:
    return f"""# DRIFT BRIGADE — Guia Rapido de Utilizacao ({version_tag})

**DRIFT BRIGADE** e um efeito de modulacao e atraso organico multibanda (4 bandas complementares) projetado para criar movimento estereo, profundidade de chorus e texturas vivas sem desfigurar o contorno da fonte sonora.

---

## Identidade Visual: Precision Instrument

A interface combina um chassi **off-white quente (`#F0EDE6`)**, paineis de controle em **grafite neutro (`#292B2D`)**, visor rebaixado quase preto (`#17191B`) e **laranja (`#F36B2B`)** como unica cor cromatica de destaque para leitura clara e conforto visual em sessoes prolongadas.

---

## Pontos de Partida: QUICK STARTS

No topo direito da interface, escolha um dos tres seletores de partida musical:

- **SUBTLE:** Movimento discreto e coeso, preservando a definicao da fonte com abertura estereo natural. Ideal para violoes, vozes, Rhodes e synths principais.
- **WIDE:** Chorus aberto e expressivo, explorando desacoplamento entre bandas (`Coherence 30%`) e largura estereo (`Width 90%`). Ideal para guitarras limpas, teclados e camadas estereo.
- **DRIFT:** Movimento organico profundo e irregular (`Chaos 82%`, `Center 18 ms`, `Feedback 38%`). Ideal para pads, cordas, texturas e ambiencias.

Apos selecionar qualquer Quick Start, todos os nove controles continuam totalmente livres para edicao e automacao.

---

## Visualizador Central: MOTION FIELD

O **Motion Field** apresenta em tempo real a representacao parametrica das **quatro bandas de frequencia** (`B1 LOW`, `B2 LO-MID`, `B3 HI-MID`, `B4 HIGH`) nos canais esquerdo e direito usando um sistema grafico monocromatico laranja:

- **Canal Esquerdo (`LEFT`):** Linha laranja continua mais espessa com ponto cheio (`●─── LEFT`).
- **Canal Direito (`RIGHT`):** Linha laranja tracejada com opacidade diferenciada e ponto anelar vazado (`○- - RIGHT`).
- **Orbita Estereo e Janela de Atraso (Direita):** Reticulo minimalista da orbita estereo das quatro bandas e barra logaritmica (`0.3 - 30 ms`) com marcador de `Center`, intervalo estimado de excursao (`EST. EXCURSION`) calculado com a taxa de amostragem real do host e indicadores de `Dynamics`, `Feedback` e sample rate.
- **Botao LIVE / HOLD:** Em `LIVE`, anima as trajetorias a 30 FPS. Em `HOLD`, congela o traco das curvas para inspecao estatica enquanto mantem todos os indicadores, valores numericos, janela de atraso e selecao de Quick Starts atualizados em tempo real.
- *Nota de honestidade visual:* O Motion Field e uma estimativa parametrica fiel do modulador e dos limites do modelo `DigitalFractional`, funcionando mesmo sem sinal de audio na entrada.

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
- **Clique na caixa numerica:** Permite digitar o valor exato pelo teclado (em `Hz`, `ms` ou `%`).
"""


def build_changelog(version_tag: str, numeric_version: str, commit_sha: str) -> str:
    today = datetime.date.today().isoformat()
    return f"""# Changelog — DRIFT BRIGADE

## [{version_tag}] — {today} (Friends Beta MVP — M3.2.1 Visual Overhaul & Polish)

Versao VST3 / CMake: `{numeric_version}`
Commit de referencia: `{commit_sha}`

### Refinado / Corrigido (M3.2.1)
- **Nova Identidade Visual (Precision Instrument):** Chassi off-white quente (`#F0EDE6`), paineis grafite neutros (`#292B2D`), interior de display quase preto (`#17191B`) e laranja (`#F36B2B`) como unica cor cromatica de destaque em toda a interface, eliminando completamente o antigo esquema ciano/ambar.
- **Motion Field Monocromatico Laranja:** Diferenciacao grafica e cromatica clara entre `LEFT` (traco laranja continuo + ponto cheio) e `RIGHT` (traco laranja tracejado + ponto vazado), orbita estereo minimalista e janela de atraso de alto contraste.
- **Modo LIVE / HOLD Desacoplado:** Em `HOLD`, o traco das trajetorias permanece congelado sem recomputacao continua, mas as leituras de parametros, `DELAY WINDOW`, `EST. EXCURSION` e o destaque dos `Quick Starts` continuam sincronizados em tempo real.
- **Excursao com Sample Rate Real do Host:** O calculo de excursao estimada do `MotionField` agora consulta de forma lock-free a taxa de amostragem efetiva preparada pelo host (`44.1`, `48`, `88.2`, `96 kHz`, etc.) em vez de assumir `48 kHz` fixo.
- **Apresentacao e Digitacao de Percentuais:** Validacao e arredondamento inteiro consistente na camada de apresentacao dos controles percentuais (`0 - 100%` e `Feedback 0 - 65%`), alem de formatacao de 2 casas decimais em `Motion` (`Hz`) e `Center` (`ms`).
- **Consistencia de Versao (`{numeric_version}` / `{version_tag}`):** Alinhamento entre `CMakeLists.txt`, metadados do bundle VST3/Standalone e empacotamento beta.

### Congelado / Preservado (DSP Freeze)
- Motor de audio publico congelado em **DigitalFractional + Wander + Gentle (4 bandas)** sem nenhuma alteracao sonora ou numerica em relacao a M3.1D / M3.2.
- Mantidos inalterados os 9 IDs de parametros, faixas internas, valores iniciais, suavizacao de 50 ms, serializacao XML (`schema=1`) e identidade VST3 (`audio.driftbrigade.m1` / `DrBg`).
- Backend experimental `Balanced1024 BBD` mantido estritamente isolado na infraestrutura interna de qualificacao.
"""


def main() -> int:
    repo_root = pathlib.Path(__file__).resolve().parent.parent
    cmake_numeric_version, cmake_release_version = read_cmake_versions(repo_root)

    parser = argparse.ArgumentParser(description="Package DriftBrigade Windows x64 Friends Beta.")
    parser.add_argument("--build-dir", required=True, type=pathlib.Path,
                        help="CMake build directory containing DriftBrigade_artefacts.")
    parser.add_argument("--output-dir", required=True, type=pathlib.Path,
                        help="Output directory for staging and final ZIP archive.")
    parser.add_argument("--config", default="Release",
                        help="Build configuration (default: Release).")
    parser.add_argument("--version", default=cmake_release_version,
                        help=f"Beta version string (default from CMakeLists.txt: {cmake_release_version}).")
    args = parser.parse_args()

    if not args.version.startswith(cmake_numeric_version):
        raise RuntimeError(
            f"Version mismatch: CMakeLists.txt version is {cmake_numeric_version}, "
            f"but --version {args.version} does not start with {cmake_numeric_version}."
        )

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
    package_folder_name = f"DriftBrigade-{args.version}-win-x64"

    args.output_dir.mkdir(parents=True, exist_ok=True)
    staging_dir = args.output_dir / package_folder_name
    if staging_dir.exists():
        shutil.rmtree(staging_dir)
    staging_dir.mkdir(parents=True, exist_ok=True)

    # 1. Copy VST3 bundle
    shutil.copytree(vst3_bundle, staging_dir / "VST3" / "DRIFT BRIGADE.vst3")

    # 2. Copy Standalone executable
    (staging_dir / "Standalone").mkdir(parents=True, exist_ok=True)
    shutil.copy2(standalone_exe, staging_dir / "Standalone" / "DRIFT BRIGADE.exe")

    # 3. Generate READMEs, CHANGELOG, VERSION.txt, and copy BETA_TEST_GUIDE.md
    write_text(staging_dir / "README_INSTALACAO.md",
               build_install_readme(args.version, cmake_numeric_version, full_sha))
    write_text(staging_dir / "README_USO.md", build_usage_readme(args.version))
    write_text(staging_dir / "CHANGELOG.md",
               build_changelog(args.version, cmake_numeric_version, full_sha))

    beta_guide_src = repo_root / "docs" / "mvp_beta_test_guide.md"
    if beta_guide_src.exists():
        shutil.copy2(beta_guide_src, staging_dir / "BETA_TEST_GUIDE.md")

    now_utc = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    version_info = (
        f"Product: DRIFT BRIGADE (Friends Beta)\n"
        f"ReleaseVersion: {args.version}\n"
        f"CMakeNumericVersion: {cmake_numeric_version}\n"
        f"Commit: {full_sha}\n"
        f"ShortCommit: {short_sha}\n"
        f"BuildConfig: {args.config}\n"
        f"Platform: Windows-x64\n"
        f"DSPBackend: DigitalFractional / Wander / Gentle (Frozen)\n"
        f"VST3BundleID: audio.driftbrigade.m1\n"
        f"VST3ManufacturerCode: Drft\n"
        f"VST3PluginCode: DrBg\n"
        f"StateSchema: 1\n"
        f"PackagedAtUTC: {now_utc}\n"
    )
    write_text(staging_dir / "VERSION.txt", version_info)

    # 4. Create ZIP archive
    zip_path = args.output_dir / f"{package_folder_name}.zip"
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
