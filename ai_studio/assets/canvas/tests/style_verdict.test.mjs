import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import {
  buildStyleVerdictCommand,
  buildStyleVerdictInstruction,
  materializeStyleVerdictImages,
  parseStyleVerdictReply,
  readStyleVerdictOutput,
  resolveCodexExecutable,
} from "../tools/style_verdict.mjs";

const EXEMPLARS = [
  { ref: "canvas://board/element/el_world", domain: "world", path: "C:/tmp/world.png" },
  { ref: "canvas://board/element/el_gui", domain: "gui", path: "C:/tmp/gui.png" },
];

test("style verdict instruction identifies target/exemplars and keeps the model advisory", () => {
  const instruction = buildStyleVerdictInstruction({
    exemplars: EXEMPLARS,
    doPrompt: "Chunky painterly forms.",
    dontPrompt: "No photorealism or text.",
  });

  assert.match(instruction, /Image 1 is the TARGET/);
  assert.match(instruction, /Image 2: owned world exemplar/);
  assert.match(instruction, /Image 3: owned gui exemplar/);
  assert.match(instruction, /Chunky painterly forms/);
  assert.match(instruction, /No photorealism or text/);
  assert.match(instruction, /advisory/i);
  assert.match(instruction, /human lead remains the acceptance backstop/i);
  assert.match(instruction, /"accept"\|"revise"\|"reject"/);
  assert.match(instruction, /do not replace deterministic alpha, crop, aspect, or palette-distance checks/i);
});

test("style verdict command attaches target then exemplars and reads the prompt from stdin", () => {
  const imagePaths = ["C:/tmp/target.png", ...EXEMPLARS.map((entry) => entry.path)];
  const command = buildStyleVerdictCommand({
    imagePaths,
    outputPath: "C:/tmp/review/last.json",
    workingDir: "C:/tmp/review",
  });
  const imageFlag = command.args.indexOf("-i");

  assert.notEqual(imageFlag, -1);
  assert.deepEqual(command.args.slice(imageFlag + 1, -1), imagePaths);
  assert.equal(command.args.at(-1), "-");
  assert.ok(command.args.includes("--output-last-message"));
  assert.ok(command.args.includes("C:/tmp/review/last.json"));
  assert.deepEqual(command.args.slice(command.args.indexOf("--sandbox"), command.args.indexOf("--sandbox") + 2), ["--sandbox", "read-only"]);
  assert.ok(command.args.includes("--ephemeral"));
  assert.ok(command.args.includes("--ignore-user-config"));
  assert.ok(command.args.includes("--ignore-rules"));
  assert.deepEqual(command.args.slice(command.args.indexOf("--cd"), command.args.indexOf("--cd") + 2), ["--cd", "C:/tmp/review"]);
  assert.doesNotMatch(command.command, /cmd(?:\.exe)?$|codex\.cmd$/i);
});

test("style verdict copies only attached images into its isolated working root", (t) => {
  const source = mkdtempSync(join(tmpdir(), "style-verdict-source-"));
  const workingDir = mkdtempSync(join(tmpdir(), "style-verdict-work-"));
  t.after(() => {
    rmSync(source, { recursive: true, force: true });
    rmSync(workingDir, { recursive: true, force: true });
  });
  const imagePaths = ["target.png", "world.png", "gui.jpg"].map((name, index) => {
    const path = join(source, name);
    writeFileSync(path, Buffer.from([index + 1]));
    return path;
  });
  const copied = materializeStyleVerdictImages({ imagePaths, workingDir });
  assert.deepEqual(copied.map((path) => path.replaceAll("\\", "/").split("/").at(-1)), ["image-1.png", "image-2.png", "image-3.jpg"]);
  assert.deepEqual(copied.map((path) => [...readFileSync(path)]), [[1], [2], [3]]);
  assert.ok(copied.every((path) => path.startsWith(workingDir)));
});

test("style verdict resolves an actual Codex executable without a Windows cmd wrapper", () => {
  const local = resolveCodexExecutable({
    platform: "win32",
    env: { LOCALAPPDATA: "C:/Local" },
    fileExists: (path) => path.replaceAll("\\", "/") === "C:/Local/OpenAI/Codex/bin/codex.exe",
  });
  assert.equal(local.replaceAll("\\", "/"), "C:/Local/OpenAI/Codex/bin/codex.exe");
  assert.equal(resolveCodexExecutable({ platform: "win32", env: {}, fileExists: () => false }), "codex.exe");
  assert.equal(resolveCodexExecutable({ platform: "linux", env: {}, fileExists: () => false }), "codex");
  assert.throws(
    () => resolveCodexExecutable({ platform: "win32", env: { CODEX_CLI_PATH: "C:/missing.exe" }, fileExists: () => false }),
    /CODEX_CLI_PATH does not exist/,
  );
});

test("style verdict reply parser accepts one fenced JSON object and fails loudly otherwise", () => {
  const report = {
    schema: "game.asset_style_verdict",
    version: 1,
    verdict: "revise",
    summary: "Needs work.",
    strengths: [],
    concerns: ["Thin silhouette"],
  };
  assert.deepEqual(parseStyleVerdictReply(`\`\`\`json\n${JSON.stringify(report)}\n\`\`\``), report);
  assert.throws(() => parseStyleVerdictReply(""), /empty result/);
  assert.throws(() => parseStyleVerdictReply("not json"), /not valid JSON/);
  assert.throws(() => parseStyleVerdictReply("[]"), /one JSON object/);
  assert.throws(() => parseStyleVerdictReply("x".repeat(65 * 1024)), /exceeds 65536 bytes/);
});

test("style verdict output reader preserves the explicit size-bound failure", (t) => {
  const root = mkdtempSync(join(tmpdir(), "style-verdict-output-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const output = join(root, "last.json");
  assert.equal(readStyleVerdictOutput(output), "");
  writeFileSync(output, "x".repeat(65 * 1024));
  assert.throws(() => readStyleVerdictOutput(output), /exceeds 65536 bytes/);
});
