import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import { studioPythonPath } from "./studio_config.mjs";

function fixture(t, pythonPath = ".venv") {
  const root = mkdtempSync(join(tmpdir(), "studio-python-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "ai_studio"), { recursive: true });
  writeFileSync(join(root, "ai_studio", "studio.config.json"), JSON.stringify({
    schema: "ai_studio.studio_config.v1", pythonPath,
  }));
  return root;
}

test("studioPythonPath resolves the configured root venv interpreter", (t) => {
  const root = fixture(t);
  const python = join(root, ".venv", "Scripts", "python.exe");
  mkdirSync(dirname(python), { recursive: true });
  writeFileSync(join(root, ".venv", "pyvenv.cfg"), "home = fixture\n");
  writeFileSync(python, "fixture");
  assert.equal(studioPythonPath(root, "win32"), python);
});

test("studioPythonPath resolves the POSIX root-venv interpreter layout", (t) => {
  const root = fixture(t);
  const python = join(root, ".venv", "bin", "python");
  mkdirSync(dirname(python), { recursive: true });
  writeFileSync(join(root, ".venv", "pyvenv.cfg"), "home = fixture\n");
  writeFileSync(python, "fixture");
  assert.equal(studioPythonPath(root, "linux"), python);
});

test("studioPythonPath rejects missing, external, and non-root-venv interpreters", (t) => {
  const missing = fixture(t);
  assert.throws(() => studioPythonPath(missing, "win32"), /not found/);

  const external = fixture(t, "C:/Python312/python.exe");
  assert.throws(() => studioPythonPath(external, "win32"), /root \.venv/);

  const other = fixture(t, "tools/.venv");
  assert.throws(() => studioPythonPath(other, "win32"), /root \.venv/);
});
