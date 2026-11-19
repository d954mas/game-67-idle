#!/usr/bin/env node
import { existsSync } from "node:fs";
import { delimiter, dirname, isAbsolute, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

import { studioPythonPath } from "./python.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const args = process.argv.slice(2);
if (!args.length) throw new Error("usage: node ai_studio/dev_environment/python_run.mjs <script-or-python-args...>");
const ambientPythonPath = String(process.env.PYTHONPATH || "");
// Python runs from the Studio root; a tool that takes paths from a game repo reads the
// caller's folder from NT_CALLER_CWD, and a script path relative to that folder still resolves.
const callerCwd = process.cwd();
if (args[0].endsWith(".py") && !isAbsolute(args[0]) && !existsSync(resolve(root, args[0]))
    && existsSync(resolve(callerCwd, args[0]))) {
  args[0] = resolve(callerCwd, args[0]);
}
const env = {
  ...process.env,
  NT_CALLER_CWD: process.env.NT_CALLER_CWD || callerCwd,
  PYTHONPATH: ambientPythonPath ? `${root}${delimiter}${ambientPythonPath}` : root,
};
const result = spawnSync(studioPythonPath(root), args, { cwd: root, env, stdio: "inherit" });
if (result.error) throw result.error;
process.exitCode = result.status ?? 1;
