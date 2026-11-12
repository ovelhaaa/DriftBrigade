"use strict";

const $ = id => document.getElementById(id);
const player = new ComparisonPlayer();
let pack, rows, current, answerKey;
let mode = modes[0], label = "X", dry = false;
let folderRevision = 0, groupRevision = 0;
let storageAvailable = true;

function element(tag, content, className) {
  const node = document.createElement(tag);
  if (content) node.textContent = content;
  if (className) node.className = className;
  return node;
}

function notice(message, error = false) {
  $("status").hidden = !message;
  $("status").textContent = message;
  $("status").classList.toggle("error", error);
}

function save() {
  try {
    localStorage.setItem(`drift-lab/${pack.sessionId}`, JSON.stringify({ csv: writeScore(rows), group: current.pair_group }));
    storageAvailable = true;
  } catch {
    storageAvailable = false;
  }
  $("save-status").textContent = storageAvailable ? "Salvo neste navegador" : "Armazenamento local indisponível. Exporte o CSV.";
  if (!storageAvailable) notice("Armazenamento local indisponível. Exporte o CSV para guardar a sessão.", true);
}

function buildNavigator() {
  $("tests").replaceChildren();
  for (const [category, [title]] of Object.entries(categories)) {
    const tests = rows.filter(row => row.set === category);
    if (!tests.length) continue;
    const section = element("section", "", `nav-category category-${category}`);
    section.append(element("h2", `${category} / ${title}`));
    for (const row of tests) {
      const id = row.pair_group;
      const button = element("button");
      button.dataset.group = id;
      const marker = element("span");
      marker.setAttribute("aria-hidden", "true");
      button.append(element("span", id, "nav-id"), element("span", pack.guide[id][3]), marker);
      button.onclick = () => openGroup(id);
      section.append(button);
    }
    $("tests").append(section);
  }
}

function updateProgress() {
  const done = rows.filter(isComplete).length;
  $("progress").max = rows.length;
  $("progress").value = done;
  $("progress-text").textContent = `${done} / ${rows.length}`;
  for (const button of $("tests").querySelectorAll("button")) {
    const row = rows.find(row => row.pair_group === button.dataset.group);
    const status = isComplete(row) ? "avaliado" : "pendente";
    button.lastElementChild.textContent = isComplete(row) ? "✓" : "○";
    button.setAttribute("aria-label", `${row.pair_group} ${pack.guide[row.pair_group][3]}, ${status}`);
    button.setAttribute("aria-current", String(!$("workspace").hidden && row === current));
  }
}

function updateScore(field, value) {
  current[field] = value;
  save();
  updateProgress();
}

function renderScore() {
  const allowed = [...pack.guide[current.pair_group][2], "no preference", "needs more listening"];
  $("preferences").replaceChildren(...allowed.map(value => {
    const wrapper = element("label");
    const input = element("input");
    Object.assign(input, { type: "radio", name: "preference", value, checked: current.preferred_candidate === value });
    input.onchange = () => updateScore("preferred_candidate", value);
    wrapper.append(input, element("span", choices[value]));
    return wrapper;
  }));
  $("metrics").replaceChildren(...scoreFields.slice(6, -1).map((field, index) => {
    const wrapper = element("label", metricNames[index], "metric");
    const select = element("select");
    for (const value of ["", "1", "2", "3", "4", "5"]) select.add(new Option(value || "—", value));
    select.value = current[field];
    select.onchange = () => updateScore(field, select.value);
    wrapper.append(select);
    return wrapper;
  }));
  $("comments").value = current.comments;
}

function renderGuide() {
  const [source, , , title, listen, preference] = pack.guide[current.pair_group];
  $("workspace").className = `workspace category-${current.set}`;
  $("test-category").textContent = `${current.pair_group} / ${categories[current.set][0]}`;
  $("test-title").textContent = title;
  $("test-description").textContent = categories[current.set][1];
  $("steps").replaceChildren(...steps.map(([heading, description], index) => {
    const li = element("li");
    const details = element("details");
    details.open = index === 0 || index === 2;
    details.append(element("summary", heading));
    if (index === 0) description = `${materials[source]} ${description}`;
    if (index === 2) description = `${listen} Pense na sua preferência: ${preference.charAt(0).toLowerCase() + preference.slice(1)}`;
    details.append(element("p", description));
    li.append(details);
    return li;
  }));
}

function renderTransport() {
  const loaded = player.buffers.size > 0;
  for (const id of ["play", "loop", "restart", "dry", "position"]) $(id).disabled = !loaded;
  $("play").textContent = player.run ? "Ⅱ Pausar" : "▶ Reproduzir";
  $("dry").setAttribute("aria-pressed", String(dry));
  $("loop").setAttribute("aria-pressed", String(player.loop));
  $("blind-state").textContent = answerKey ? "Gabarito revelado" : "Teste cego";
  for (const button of document.querySelectorAll("[data-mode]")) {
    button.setAttribute("aria-pressed", String(button.dataset.mode === mode));
  }
  for (const button of $("candidates").children) {
    button.disabled = !loaded;
    button.setAttribute("aria-pressed", String(!dry && button.dataset.label === label));
  }
  $("mode-description").textContent = mode === modes[0]
    ? "Comece aqui. Um ajuste fixo aproxima o volume dos candidatos para destacar as diferenças de timbre e movimento."
    : "Ouça as diferenças de nível originais entre os candidatos. Os rótulos X, Y e Z permanecem os mesmos.";
}

function redraw() {
  drawReference($("wave"), player.buffers.get("dry"));
}

async function openGroup(id) {
  const row = rows.find(row => row.pair_group === id);
  const changed = row !== current;
  $("workspace").hidden = false;
  $("results").hidden = true;
  if (!changed) {
    updateProgress();
    redraw();
    return;
  }
  const revision = ++groupRevision;
  current = row;
  label = "X";
  dry = false;
  player.active = `${mode}/X`;
  $("candidates").replaceChildren(...[...pack.guide[id][2]].map(value => {
    const button = element("button", value);
    button.dataset.label = value;
    button.onclick = () => selectSignal(value);
    return button;
  }));
  $("next").textContent = row === rows.at(-1) ? "Resultados da audição →" : "Próximo teste →";
  renderGuide();
  renderScore();
  updateProgress();
  save();
  $("audio-status").textContent = "Carregando a comparação…";
  const load = player.load(comparisonFiles(row), pack.get);
  renderTransport();
  redraw();
  try {
    if (await load && revision === groupRevision) {
      $("audio-status").textContent = "";
      renderTransport();
      redraw();
    }
  } catch (error) {
    if (revision === groupRevision) $("audio-status").textContent = error.message;
  }
}

async function openFolder(files) {
  const revision = ++folderRevision;
  notice("Verificando o pacote…");
  try {
    const nextPack = await openPackage(files, packageProfiles);
    if (revision !== folderRevision) return;
    let nextRows = nextPack.rows, group = nextRows[0].pair_group, restored = false;
    storageAvailable = true;
    try {
      const saved = localStorage.getItem(`drift-lab/${nextPack.sessionId}`);
      if (saved) {
        const session = JSON.parse(saved);
        nextRows = readScore(session.csv, nextPack.guide);
        if (nextRows.some(row => row.pair_group === session.group)) group = session.group;
        restored = true;
      }
    } catch {
      storageAvailable = false;
    }
    pack = nextPack;
    $("comments").placeholder = pack.version === "M3.1B.1"
      ? "LEVEL MATCHED: timbre e movimento. RAW: nível, ruído e cauda."
      : "Trechos, texturas ou diferenças entre modos…";
    rows = nextRows;
    current = undefined;
    answerKey = undefined;
    mode = modes[0];
    $("key-status").textContent = "";
    $("welcome").hidden = true;
    $("lab").hidden = false;
    $("results-button").disabled = false;
    $("package-label").textContent = `${pack.version} · ${pack.count} WAVs · ${pack.dryCount} ${pack.dryCount === 1 ? "original" : "originais"} · PCM24 / 48 kHz`;
    buildNavigator();
    notice(restored ? "Sessão anterior retomada." : storageAvailable ? "" : "Não foi possível ler a sessão anterior.", !storageAvailable);
    openGroup(group);
  } catch (error) {
    if (revision === folderRevision) notice(error.message, true);
  }
}

function setMode(value) {
  mode = value;
  if (!dry) {
    const slot = `${mode}/${label}`;
    if (player.buffers.size) player.select(slot);
    else player.active = slot;
  }
  renderTransport();
}

async function play() {
  try {
    await player.play();
  } catch {
    $("audio-status").textContent = "O navegador bloqueou a reprodução. Pressione Reproduzir para tentar novamente.";
  }
  renderTransport();
}

function selectSignal(value) {
  if (!player.buffers.size || (value !== "dry" && !pack.guide[current.pair_group][2].includes(value))) return;
  dry = value === "dry";
  if (!dry) label = value;
  player.select(dry ? "dry" : `${mode}/${label}`);
  renderTransport();
  if (dry) play();
}

function togglePlay() {
  if (player.run) {
    player.stop();
    renderTransport();
  } else play();
}

function keyNames(row) {
  const factor = categories[row.set][2];
  return row.blind_candidates.split(";").map(name => {
    const entry = answerKey.get(name);
    const value = entry[factor];
    const variant = value === "ExperimentalBBD" ? "BBD completo" : factor === "stages" ? `${value} estágios` : value;
    return { label: name.at(-5), variant, id: entry.candidate_id };
  });
}

function resultGroups() {
  return Object.entries(categories).map(([category, [title, description]]) => {
    const tests = rows.filter(row => row.set === category).map(row => {
      const mappings = answerKey ? keyNames(row) : [];
      const selection = row.preferred_candidate;
      const picked = mappings.find(item => item.label === selection)?.variant;
      const preference = choices[selection] || "Sem registro";
      const strength = row.preference_strength_1_5;
      return { row, mappings, picked, summary: `${isComplete(row) ? "Concluído" : "Pendente"} · ${preference}${picked ? ` (${picked})` : ""}${strength ? ` · intensidade ${strength}/5` : ""}` };
    });
    const pending = tests.filter(test => !isComplete(test.row)).map(test => test.row.pair_group);
    const neutral = tests.filter(test => test.row.preferred_candidate === "no preference").map(test => test.row.pair_group);
    const picks = new Set(tests.map(test => test.picked).filter(Boolean));
    const notes = [];
    if (pending.length) notes.push(`Ainda em aberto: ${pending.join(", ")}.`);
    if (neutral.length) notes.push(`Sem preferência: ${neutral.join(", ")}.`);
    if (picks.size > 1) notes.push(`Preferências divergentes entre cenas: ${[...picks].join(" / ")}. Considere a fonte, a intensidade e os comentários de cada teste.`);
    return { category, title, description, tests, notes };
  }).filter(group => group.tests.length);
}

function showResults() {
  player.stop();
  renderTransport();
  $("workspace").hidden = true;
  $("results").hidden = false;
  updateProgress();
  const done = rows.filter(isComplete).length;
  $("result-count").textContent = `${done} concluídos · ${rows.length - done} pendentes`;
  $("reveal").disabled = Boolean(answerKey);
  $("result-list").replaceChildren(...resultGroups().map(({ category, title, description, tests, notes }) => {
    const section = element("section", "", `panel result-category category-${category}`);
    section.append(element("h2", `${category} / ${title}`));
    if (answerKey) section.append(element("p", description, "muted"));
    for (const note of notes) section.append(element("p", note));
    for (const { row, mappings, summary } of tests) {
      const id = row.pair_group;
      const article = element("article", "", "result-row");
      const button = element("button", `${id} · ${pack.guide[id][3]}`);
      button.onclick = () => openGroup(id);
      const heading = element("h3");
      heading.append(button);
      article.append(heading, element("p", summary));
      if (mappings.length) article.append(element("p", mappings.map(item => `${item.label} — ${item.variant} · ${item.id}`).join("\n"), "muted"));
      if (row.comments) article.append(element("p", row.comments));
      section.append(article);
    }
    return section;
  }));
}

function download(name, data, type) {
  const url = URL.createObjectURL(new Blob([data], { type }));
  const link = element("a");
  link.href = url;
  link.download = name;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function markdownText(value) {
  return value.replace(/[\\`*_{}\[\]()#+.!|<>~-]/g, "\\$&");
}

function report() {
  const done = rows.filter(isComplete).length;
  const lines = [`# DriftBrigade ${pack.version} — Resultados da audição`, "",
    `${done} concluídos · ${rows.length - done} pendentes`, "",
    answerKey ? "Gabarito revelado." : "Sessão cega.", ""];
  for (const { category, title, description, tests, notes } of resultGroups()) {
    lines.push(`## ${category} — ${title}`, "");
    if (answerKey) lines.push(description, "");
    for (const note of notes) lines.push(note, "");
    for (const { row, mappings, summary } of tests) {
      lines.push(`### ${row.pair_group} — ${pack.guide[row.pair_group][3]}`, "", summary, "");
      for (const item of mappings) lines.push(`- ${item.label} — ${item.variant} (${markdownText(item.id)})`);
      if (mappings.length) lines.push("");
      for (const [index, field] of scoreFields.slice(7, -1).entries()) {
        if (row[field]) lines.push(`- ${metricNames[index + 1]}: ${row[field]}/5`);
      }
      if (row.comments) lines.push("", ...row.comments.split(/\r\n|\n|\r/).map(line => `> ${markdownText(line)}`), "");
      lines.push("");
    }
  }
  return lines.join("\n");
}

$("folder-button").onclick = $("welcome-folder").onclick = () => $("folder").click();
$("folder").onchange = event => {
  const files = Array.from(event.target.files);
  event.target.value = "";
  if (files.length) openFolder(files);
};
$("results-button").onclick = showResults;
$("play").onclick = togglePlay;
$("dry").onclick = () => selectSignal("dry");
$("restart").onclick = () => player.seek(0);
$("loop").onclick = () => { player.setLoop(!player.loop); renderTransport(); };
$("position").oninput = event => player.seek(Number(event.target.value));
$("volume").oninput = event => player.setVolume(Number(event.target.value));
for (const button of document.querySelectorAll("[data-mode]")) button.onclick = () => setMode(button.dataset.mode);
$("comments").oninput = event => updateScore("comments", event.target.value);
$("next").onclick = () => {
  const next = rows[rows.indexOf(current) + 1];
  if (next) {
    openGroup(next.pair_group);
    $("test-title").scrollIntoView({ block: "nearest" });
  } else showResults();
};
$("export-csv").onclick = () => download("LISTENING_SCORECARD.csv", writeScore(rows), "text/csv;charset=utf-8");
$("export-md").onclick = () => download(`${pack.version}_LISTENING_RESULTS.md`, report(), "text/markdown;charset=utf-8");
$("import").onclick = () => $("import-file").click();
$("import-file").onchange = async event => {
  const [file] = event.target.files;
  event.target.value = "";
  if (!file) return;
  const targetPack = pack;
  try {
    const imported = readScore(await file.text(), pack.guide);
    if (pack !== targetPack) return;
    if (rows.some(row => scoreFields.slice(5).some(field => row[field])) &&
        writeScore(rows) !== writeScore(imported) && !confirm("Substituir as avaliações atuais pelo CSV importado?")) return;
    rows = imported;
    current = rows.find(row => row.pair_group === current.pair_group);
    renderScore();
    notice("Avaliação importada.");
    save();
    showResults();
  } catch (error) {
    notice(`Falha ao importar. ${error.message}`, true);
  }
};
$("reveal").onclick = () => {
  $("confirm").returnValue = "";
  $("confirm").showModal();
};
$("confirm").onclose = async () => {
  if ($("confirm").returnValue !== "reveal") return;
  const targetPack = pack;
  const keyFile = pack.get("ANSWER_KEY.csv");
  if (!keyFile) {
    $("key-status").textContent = "ANSWER_KEY.csv ausente. As identidades continuam indisponíveis.";
    return;
  }
  try {
    const text = await keyFile.text();
    if (pack !== targetPack) return;
    answerKey = readAnswerKey(text, rows);
    $("key-status").textContent = "Gabarito revelado. As preferências abaixo são as que você registrou.";
    showResults();
  } catch (error) {
    if (pack === targetPack) $("key-status").textContent = `Gabarito indisponível. ${error.message}`;
  }
};

document.addEventListener("keydown", event => {
  if ($("lab").hidden || $("workspace").hidden || $("confirm").open || !player.buffers.size ||
      event.altKey || event.ctrlKey || event.metaKey || event.repeat ||
      event.target.closest("input, textarea, select, [contenteditable]")) return;
  const key = event.key.toLowerCase();
  const actions = {
    " ": togglePlay, "1": () => selectSignal("X"), "2": () => selectSignal("Y"),
    "3": () => selectSignal("Z"), "0": () => selectSignal("dry"),
    r: $("restart").onclick, l: $("loop").onclick,
    m: () => setMode(mode === modes[0] ? modes[1] : modes[0]),
  };
  if (key === " " && event.target.closest("button, summary, a")) return;
  if (Object.hasOwn(actions, key)) {
    event.preventDefault();
    actions[key]();
  }
});

new ResizeObserver(redraw).observe($("wave"));
function updateClock() {
  if (!$("workspace").hidden && !$("lab").hidden) {
    const position = player.position;
    $("position").value = position;
    $("clock").textContent = `${position.toFixed(1).padStart(4, "0")} / 10.0 s`;
    const width = $("wave").clientWidth;
    if (width > 52) $("wave").nextElementSibling.style.left = `${(26 + (width - 52) * position / 10) / width * 100}%`;
    if (!player.run && $("play").textContent.includes("Pausar")) renderTransport();
  }
  requestAnimationFrame(updateClock);
}
requestAnimationFrame(updateClock);
