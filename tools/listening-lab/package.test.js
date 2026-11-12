"use strict";

const { test } = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs/promises");
const path = require("node:path");
const { guide, voiceGuide, packageProfiles } = require("./guide");
const { scoreFields, readScore, writeScore, comparisonFiles, inspectWav, openPackage, readAnswerKey, isComplete } = require("./package");

const artifact = process.env.M3_ARTIFACT;
const template = Object.entries(guide).map(([id, [source, scene, labels]]) => ({
  ...Object.fromEntries(scoreFields.map(field => [field, ""])),
  set: id[0], pair_group: id, source, scene,
  blind_candidates: [...labels].map(label => `SET_${id}_${label}.wav`).join(";"),
}));

test("12 public groups map to 26 blind candidates and 3 dry references", () => {
  const rows = readScore(writeScore(template), guide);
  const paths = new Set(rows.flatMap(row => comparisonFiles(row).map(([, name]) => name)));
  assert.equal(rows.length, 12);
  assert.equal([...paths].filter(name => name.startsWith("raw/")).length, 26);
  assert.equal([...paths].filter(name => name.startsWith("level_matched/")).length, 26);
  assert.deepEqual([...paths].filter(name => name.startsWith("dry/")), ["dry/DRY_A.wav", "dry/DRY_B.wav", "dry/DRY_C.wav"]);
  assert.deepEqual(comparisonFiles(rows.find(row => row.pair_group === "D02")), [
    ["dry", "dry/DRY_C.wav"],
    ["level_matched/X", "level_matched/SET_D02_X.wav"],
    ["level_matched/Y", "level_matched/SET_D02_Y.wav"],
    ["level_matched/Z", "level_matched/SET_D02_Z.wav"],
    ["raw/X", "raw/SET_D02_X.wav"], ["raw/Y", "raw/SET_D02_Y.wav"], ["raw/Z", "raw/SET_D02_Z.wav"],
  ]);
});

test("scorecard preserves schema, blank fields, Portuguese and quoted multiline notes", () => {
  const rows = structuredClone(template);
  rows[0].preferred_candidate = "no preference";
  rows[0].comments = 'Transição, "leve"\r\nCauda longa.';
  rows[1].preferred_candidate = "needs more listening";
  rows[2].preferred_candidate = "Y";
  rows[2].preference_strength_1_5 = "5";
  assert.deepEqual(readScore("\uFEFF" + writeScore(rows), guide), rows);
  assert.deepEqual(rows.slice(0, 4).map(isComplete), [true, false, true, false]);
  rows[0].preferred_candidate = "Z";
  assert.throws(() => readScore(writeScore(rows), guide), /nota inválida/);
  assert.throws(() => readScore(writeScore(template.slice(1)), guide), /12 grupos/);
  assert.throws(() => readScore(writeScore(template).replace("subtle", "wide"), guide), /Metadados/);
  assert.throws(() => readScore(writeScore(template) + '"incompleto', guide), /incompleto/);
});

test("rejects corrupt or truncated WAV headers", () => {
  const header = new ArrayBuffer(44);
  const bytes = 2880044;
  const view = new DataView(header);
  for (const [offset, tag] of [[0, "RIFF"], [8, "WAVEfmt "], [36, "data"]]) {
    new Uint8Array(header).set(Buffer.from(tag), offset);
  }
  for (const [offset, value] of [[4, bytes - 8], [16, 16], [24, 48000], [28, 288000], [40, bytes - 44]]) view.setUint32(offset, value, true);
  for (const [offset, value] of [[20, 1], [22, 2], [32, 6], [34, 24]]) view.setUint16(offset, value, true);
  assert.doesNotThrow(() => inspectWav(header, bytes));
  assert.throws(() => inspectWav(header, bytes - 1), /PCM24/);
  view.setUint16(34, 16, true);
  assert.throws(() => inspectWav(header, bytes), /PCM24/);
});

for (const [version, directory, count, candidateCount, groupIds] of [
  ["M3.0", artifact, 55, 26, Object.keys(guide)],
  ["M3.1B.1", process.env.M31_VOICE_ARTIFACT, 13, 6, Object.keys(voiceGuide)],
]) test(`opens ${version} without reading identity documents`, { skip: !directory }, async () => {
  const files = [];
  const blocked = new Set(["ANSWER_KEY.csv", "listening_render_manifest.csv"]);
  for (const name of await fs.readdir(directory, { recursive: true })) {
    const fullPath = path.join(directory, name);
    const info = await fs.stat(fullPath);
    if (!info.isFile()) continue;
    files.push({
      webkitRelativePath: `outer/package/${name.replaceAll("\\", "/")}`,
      size: info.size,
      lastModified: info.mtimeMs,
      text: async () => {
        assert(!blocked.has(name), `Premature read of ${name}`);
        return fs.readFile(fullPath, "utf8");
      },
      slice: (start, end) => ({
        async arrayBuffer() {
          const handle = await fs.open(fullPath);
          try {
            const result = new ArrayBuffer(end - start);
            await handle.read(new DataView(result), 0, end - start, start);
            return result;
          } finally { await handle.close(); }
        },
      }),
    });
  }
  const pack = await openPackage(files, packageProfiles);
  assert.equal(pack.version, version);
  assert.equal(pack.count, count);
  assert.deepEqual(pack.rows.map(row => row.pair_group), groupIds);
  const edited = structuredClone(pack.rows);
  edited[0].preferred_candidate = "Y";
  edited[0].preference_strength_1_5 = "4";
  edited[0].comments = "LEVEL MATCHED: natural.\nRAW: cauda discreta.";
  assert.deepEqual(readScore(writeScore(edited), pack.guide), edited);
  const keyText = await fs.readFile(path.join(directory, "ANSWER_KEY.csv"), "utf8");
  assert.equal(readAnswerKey(keyText, pack.rows).size, candidateCount);
  assert.throws(() => readAnswerKey(keyText.replace(`SET_${groupIds[0]}_X.wav`, `SET_${groupIds[0]}_Y.wav`), pack.rows), /incompatível/);
  assert.throws(() => readAnswerKey(keyText.trimEnd().split(/\r?\n/).slice(0, -1).join("\n"), pack.rows), /incompleto/);
  const dry = files.find(file => file.webkitRelativePath.endsWith("DRY_C.wav"));
  await assert.rejects(openPackage(files.filter(file => file !== dry), packageProfiles), /WAVs ausentes.*DRY_C/);
  dry.size--;
  await assert.rejects(openPackage(files, packageProfiles), /WAVs inválidos.*DRY_C/);
});
