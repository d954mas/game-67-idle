#!/usr/bin/env node
import { delimiter, dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

import { studioPythonPath } from "./python.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const args = process.argv.slice(2);
if (!args.length) throw new Error("usage: node ai_studio/dev_environment/python_run.mjs <script-or-python-args...>");
const ambientPythonPath = String(process.env.PYTHONPATH || "");
const env = {
  ...process.env,
  PYTHONPATH: ambientPythonPath ? `${root}${delimiter}${ambientPythonPath}` : root,
};
const result = spawnSync(studioPythonPath(root), args, { cwd: root, env, stdio: "inherit" });
if (result.error) throw result.error;
process.exitCode = result.status ?? 1;
