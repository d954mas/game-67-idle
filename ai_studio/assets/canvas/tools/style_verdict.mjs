import { execFile } from "node:child_process";
import { copyFileSync, existsSync, mkdtempSync, readFileSync, rmSync, statSync } from "node:fs";
import { tmpdir } from "node:os";
import { extname, join } from "node:path";
import { promisify } from "node:util";

const execFileAsync = promisify(execFile);
const CODEX_TIMEOUT_MS = 300_000;
const MAX_REPLY_BYTES = 64 * 1024;

export function resolveCodexExecutable({
  platform = process.platform,
  env = process.env,
  fileExists = existsSync,
} = {}) {
  const configured = String(env.CODEX_CLI_PATH || "").trim();
  if (configured) {
    if (!fileExists(configured)) throw new Error(`CODEX_CLI_PATH does not exist: ${configured}`);
    return configured;
  }
  if (platform === "win32") {
    const local = env.LOCALAPPDATA ? join(env.LOCALAPPDATA, "OpenAI", "Codex", "bin", "codex.exe") : "";
    if (local && fileExists(local)) return local;
    return "codex.exe";
  }
  return "codex";
}

function stripCodeFence(text) {
  const trimmed = String(text || "").trim();
  const fenced = /^```(?:json)?\s*([\s\S]*?)\s*```$/i.exec(trimmed);
  return fenced ? fenced[1].trim() : trimmed;
}

export function buildStyleVerdictInstruction({ exemplars, doPrompt, dontPrompt } = {}) {
  if (!Array.isArray(exemplars) || exemplars.length < 2 || exemplars.length > 3) {
    throw new Error("buildStyleVerdictInstruction requires 2-3 exemplars");
  }
  if (typeof doPrompt !== "string" || !doPrompt.trim()) {
    throw new Error("buildStyleVerdictInstruction requires doPrompt");
  }
  if (typeof dontPrompt !== "string" || !dontPrompt.trim()) {
    throw new Error("buildStyleVerdictInstruction requires dontPrompt");
  }
  const exemplarLines = exemplars.map((entry, index) => (
    `Image ${index + 2}: owned ${entry.domain} exemplar (${entry.ref}).`
  )).join("\n");
  return (
    "Act as an advisory game-art style reviewer. Image 1 is the TARGET under review.\n" +
    `${exemplarLines}\n\n` +
    `DO / desired direction:\n${doPrompt.trim()}\n\n` +
    `DON'T / prohibited drift:\n${dontPrompt.trim()}\n\n` +
    "Compare the target visually against every owned exemplar and the written Do/Don't. " +
    "Judge style fit only; do not replace deterministic alpha, crop, aspect, or palette-distance checks. " +
    "This verdict is advisory and a human lead remains the acceptance backstop.\n\n" +
    "Return ONLY one JSON object with EXACTLY these keys:\n" +
    '"schema":"game.asset_style_verdict",\n' +
    '"version":1,\n' +
    '"verdict":"accept"|"revise"|"reject",\n' +
    '"summary":"one concise sentence",\n' +
    '"strengths":["0-8 concise visual observations"],\n' +
    '"concerns":["0-8 concise actionable visual observations"].\n' +
    "Use accept when the target fits the locked direction, revise for correctable drift, and reject for a conflicting direction. " +
    "No markdown fence, preamble, scores, embeddings, or extra keys."
  );
}

export function buildStyleVerdictCommand({ imagePaths, outputPath, workingDir } = {}) {
  if (!Array.isArray(imagePaths) || imagePaths.length < 3 || imagePaths.length > 4
      || imagePaths.some((path) => typeof path !== "string" || !path)) {
    throw new Error("buildStyleVerdictCommand requires target plus 2-3 image paths");
  }
  if (!outputPath) throw new Error("buildStyleVerdictCommand requires outputPath");
  if (!workingDir) throw new Error("buildStyleVerdictCommand requires workingDir");
  return {
    command: resolveCodexExecutable(),
    args: [
      "exec",
      "--sandbox",
      "read-only",
      "--ephemeral",
      "--ignore-user-config",
      "--ignore-rules",
      "--cd",
      workingDir,
      "--skip-git-repo-check",
      "--output-last-message",
      outputPath,
      "-i",
      ...imagePaths,
      "-",
    ],
  };
}

export function materializeStyleVerdictImages({ imagePaths, workingDir } = {}) {
  if (!Array.isArray(imagePaths) || imagePaths.length < 3 || imagePaths.length > 4) {
    throw new Error("materializeStyleVerdictImages requires target plus 2-3 image paths");
  }
  if (!workingDir) throw new Error("materializeStyleVerdictImages requires workingDir");
  return imagePaths.map((source, index) => {
    const extension = extname(source).toLowerCase() || ".png";
    const destination = join(workingDir, `image-${index + 1}${extension}`);
    copyFileSync(source, destination);
    return destination;
  });
}

export function readStyleVerdictOutput(outputPath) {
  if (!existsSync(outputPath)) return "";
  if (statSync(outputPath).size > MAX_REPLY_BYTES) {
    throw new Error(`runStyleVerdict: codex reply exceeds ${MAX_REPLY_BYTES} bytes`);
  }
  return readFileSync(outputPath, "utf8");
}

export function parseStyleVerdictReply(raw) {
  if (Buffer.byteLength(String(raw || ""), "utf8") > MAX_REPLY_BYTES) {
    throw new Error(`runStyleVerdict: codex reply exceeds ${MAX_REPLY_BYTES} bytes`);
  }
  const stripped = stripCodeFence(raw);
  if (!stripped) throw new Error("runStyleVerdict: codex returned an empty result");
  let parsed;
  try {
    parsed = JSON.parse(stripped);
  } catch {
    throw new Error(`runStyleVerdict: codex reply was not valid JSON: ${JSON.stringify(stripped.slice(0, 160))}`);
  }
  if (!parsed || typeof parsed !== "object" || Array.isArray(parsed)) {
    throw new Error("runStyleVerdict: codex reply must be one JSON object");
  }
  return parsed;
}

export async function runStyleVerdict({ targetPath, exemplars, doPrompt, dontPrompt } = {}) {
  if (!targetPath) throw new Error("runStyleVerdict requires targetPath");
  const instruction = buildStyleVerdictInstruction({ exemplars, doPrompt, dontPrompt });
  const workDir = mkdtempSync(join(tmpdir(), "canvas-style-verdict-"));
  try {
    const outputPath = join(workDir, "last.json");
    const sourcePaths = [targetPath, ...exemplars.map((entry) => entry.path)];
    const imagePaths = materializeStyleVerdictImages({ imagePaths: sourcePaths, workingDir: workDir });
    const { command, args } = buildStyleVerdictCommand({ imagePaths, outputPath, workingDir: workDir });
    const promise = execFileAsync(command, args, {
      cwd: workDir,
      timeout: CODEX_TIMEOUT_MS,
      maxBuffer: 16 * 1024 * 1024,
    });
    if (promise.child?.stdin) {
      promise.child.stdin.write(instruction);
      promise.child.stdin.end();
    }
    await promise;
    const raw = readStyleVerdictOutput(outputPath);
    return parseStyleVerdictReply(raw);
  } finally {
    rmSync(workDir, { recursive: true, force: true });
  }
}
