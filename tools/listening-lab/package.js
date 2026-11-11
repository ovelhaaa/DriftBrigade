"use strict";

const scoreFields = [
  "set", "pair_group", "source", "scene", "blind_candidates", "preferred_candidate",
  "preference_strength_1_5", "modulation_naturalness_1_5", "stereo_quality_1_5",
  "tonal_quality_1_5", "transient_quality_1_5", "tail_quality_1_5", "artifacts_1_5",
  "fatigue_1_5", "comments",
];
const keyFields = ["group", "blind_filename", "candidate_id", "backend", "stages", "gain_profile", "organic_variant", "bank_mode"];
const modes = ["level_matched", "raw"];
const choices = {
  X: "X", Y: "Y", Z: "Z",
  "no preference": "Sem preferência",
  "needs more listening": "Preciso ouvir novamente",
};
const metricNames = [
  "Intensidade da preferência", "Naturalidade da modulação", "Imagem estéreo",
  "Qualidade tonal", "Preservação de transientes", "Cauda e feedback",
  "Artefatos perceptíveis", "Fadiga auditiva",
];

function parseCsv(input, fields) {
  const rows = [];
  let row = [], value = "", state = "start";
  const finishField = () => { row.push(value); value = ""; state = "start"; };
  const finishRow = () => { finishField(); rows.push(row); row = []; };
  const csv = input.replace(/^\uFEFF/, "");
  for (let i = 0; i < csv.length; i++) {
    const char = csv[i];
    if (state === "quoted") {
      if (char !== '"') value += char;
      else if (csv[i + 1] === '"') { value += '"'; i++; }
      else state = "closed";
    } else if (char === ",") {
      finishField();
    } else if (char === "\r" || char === "\n") {
      finishRow();
      if (char === "\r" && csv[i + 1] === "\n") i++;
    } else if (char === '"' && state === "start") {
      state = "quoted";
    } else {
      if (state === "closed" || char === '"') throw new Error("Aspas inválidas no CSV.");
      value += char;
      state = "plain";
    }
  }
  if (state === "quoted") throw new Error("Campo entre aspas incompleto.");
  if (row.length || state !== "start") finishRow();
  const header = rows.shift();
  if (header?.join(",") !== fields.join(",")) throw new Error("Colunas CSV incompatíveis com a M3.0.");
  return rows.map(values => {
    if (values.length !== fields.length) throw new Error("Linha CSV com número incorreto de campos.");
    return Object.fromEntries(fields.map((field, index) => [field, values[index]]));
  });
}

function writeScore(rows) {
  const lines = rows.map(row => scoreFields.map(field => '"' + row[field].replaceAll('"', '""') + '"').join(","));
  return [scoreFields.join(","), ...lines, ""].join("\r\n");
}

function namesFor(id, labels) {
  return [...labels].map(label => `SET_${id}_${label}.wav`);
}

function readScore(csv, guide) {
  const byId = new Map();
  for (const row of parseCsv(csv, scoreFields)) {
    const id = row.pair_group;
    if (!Object.hasOwn(guide, id) || byId.has(id)) throw new Error("Grupo desconhecido ou duplicado no scorecard.");
    const [source, scene, labels] = guide[id];
    if (row.set !== id[0] || row.source !== source || row.scene !== scene ||
        row.blind_candidates !== namesFor(id, labels).join(";")) {
      throw new Error(`Metadados incompatíveis no grupo ${id}.`);
    }
    const allowed = ["", ...labels, "no preference", "needs more listening"];
    if (!allowed.includes(row.preferred_candidate) ||
        scoreFields.slice(6, -1).some(field => row[field] && !/^[1-5]$/.test(row[field]))) {
      throw new Error(`Preferência ou nota inválida no grupo ${id}.`);
    }
    byId.set(id, row);
  }
  if (byId.size !== Object.keys(guide).length) throw new Error(`O scorecard deve conter os ${Object.keys(guide).length} grupos do pacote.`);
  return Object.keys(guide).map(id => byId.get(id));
}

function comparisonFiles(row) {
  return [
    ["dry", `dry/DRY_${row.source}.wav`],
    ...modes.flatMap(mode => row.blind_candidates.split(";").map(name => [`${mode}/${name.at(-5)}`, `${mode}/${name}`])),
  ];
}

function inspectWav(header, bytes) {
  // The M3.0 writer emits a canonical 44-byte RIFF PCM header.
  const data = new DataView(header);
  const tag = (offset, expected) => [...expected].every((char, i) => data.getUint8(offset + i) === char.charCodeAt(0));
  if (data.byteLength !== 44 || bytes !== 2880044 ||
      !tag(0, "RIFF") || !tag(8, "WAVEfmt ") || !tag(36, "data") ||
      data.getUint32(4, true) !== bytes - 8 || data.getUint32(16, true) !== 16 ||
      data.getUint16(20, true) !== 1 || data.getUint16(22, true) !== 2 ||
      data.getUint32(24, true) !== 48000 || data.getUint32(28, true) !== 288000 ||
      data.getUint16(32, true) !== 6 || data.getUint16(34, true) !== 24 ||
      data.getUint32(40, true) !== bytes - 44) {
    throw new Error("Esperado WAV M3.0 estéreo PCM24, 48 kHz, 10 segundos.");
  }
}

async function openPackage(selected, profiles) {
  const files = new Map(Array.from(selected, file => [file.webkitRelativePath, file]));
  const suffix = "/LISTENING_SCORECARD.csv";
  const roots = [...files.keys()].filter(path => path.endsWith(suffix));
  if (roots.length !== 1) throw new Error("Selecione uma pasta com exatamente um LISTENING_SCORECARD.csv.");
  const root = roots[0].slice(0, -suffix.length);
  const get = path => files.get(`${root}/${path}`);
  const score = await get("LISTENING_SCORECARD.csv").text();
  const firstGroup = parseCsv(score, scoreFields)[0]?.pair_group;
  const profile = profiles.find(({ guide }) => Object.hasOwn(guide, firstGroup));
  if (!profile) throw new Error("O scorecard não corresponde a um pacote conhecido.");
  const rows = readScore(score, profile.guide);
  const paths = new Set(rows.flatMap(row => comparisonFiles(row).map(([, path]) => path)));
  const missing = [...paths].filter(path => !get(path));
  if (missing.length) throw new Error(`WAVs ausentes (${missing.length}): ${missing.slice(0, 4).join(", ")}`);
  const invalid = await Promise.all([...paths].map(async path => {
    const file = get(path);
    try { inspectWav(await file.slice(0, 44).arrayBuffer(), file.size); }
    catch { return path; }
    return "";
  }));
  const errors = invalid.filter(Boolean);
  if (errors.length) throw new Error(`WAVs inválidos (${errors.length}): ${errors.slice(0, 4).join(", ")}. Esperado PCM24 estéreo, 48 kHz, 10 s.`);
  const checksums = await get("SHA256SUMS.txt")?.text();
  const fingerprint = checksums || [...paths].map(path => `${path}:${get(path).lastModified}`).join("\n");
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(fingerprint));
  const sessionId = Array.from(new Uint8Array(digest), byte => byte.toString(16).padStart(2, "0")).join("");
  return { rows, get, sessionId, count: paths.size, dryCount: new Set(rows.map(row => row.source)).size, ...profile };
}

function readAnswerKey(csv, rows) {
  const expected = new Map(rows.flatMap(row => row.blind_candidates.split(";").map(name => [name, row])));
  const key = new Map();
  const variants = {
    A: ["DigitalFractional", "ExperimentalBBD"], B: ["512", "1024"],
    C: ["Conservative", "Nominal"], D: ["Wander", "PaperNarrowband", "PhaseDrift"],
    E: ["Gentle", "Selective"],
  };
  const factor = { A: "backend", B: "stages", C: "gain_profile", D: "organic_variant", E: "bank_mode" };
  for (const entry of parseCsv(csv, keyFields)) {
    const row = expected.get(entry.blind_filename);
    if (!row || row.pair_group !== entry.group || key.has(entry.blind_filename) ||
        !entry.candidate_id || !variants[row.set].includes(entry[factor[row.set]])) {
      throw new Error("O gabarito contém um mapeamento incompatível com o pacote.");
    }
    key.set(entry.blind_filename, entry);
  }
  if (key.size !== expected.size || rows.some(row => new Set(row.blind_candidates.split(";")
      .map(name => key.get(name)[factor[row.set]])).size !== variants[row.set].length)) {
    throw new Error("O gabarito está incompleto ou repete uma implementação dentro de um grupo.");
  }
  return key;
}

function isComplete(row) {
  return Boolean(row.preferred_candidate && row.preferred_candidate !== "needs more listening");
}

if (typeof module !== "undefined") {
  module.exports = { scoreFields, parseCsv, writeScore, readScore, comparisonFiles, inspectWav, openPackage, readAnswerKey, isComplete };
}
