import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..', '..');

const scripts = [
  'validate_gdd.mjs',
  'validate_site.mjs',
  'simulate_balance.mjs',
  'validate_reducer_vectors.mjs',
  'validate_assets.mjs',
  'validate_analytics.mjs',
  'validate_ui_flow.mjs',
  'validate_ui_components.mjs',
  'validate_asset_generation_queue.mjs',
  'validate_ready_assets.mjs',
  'validate_technical_blueprint.mjs',
  'validate_release_readiness.mjs',
  'validate_implementation_tasks.mjs',
  'validate_risk_register.mjs',
  'validate_playtest_observation_schema.mjs',
  'validate_handoff_status.mjs',
  'validate_runtime_evidence_manifest.mjs',
  'validate_build_readiness.mjs'
];

function run(script) {
  return new Promise((resolve) => {
    const scriptPath = path.join(root, 'gamedesing', 'tools', script);
    const child = spawn(process.execPath, [scriptPath], { cwd: root });
    let stdout = '';
    let stderr = '';
    child.stdout.on('data', (chunk) => {
      stdout += chunk;
    });
    child.stderr.on('data', (chunk) => {
      stderr += chunk;
    });
    child.on('close', (code) => {
      resolve({ script, code, stdout: stdout.trim(), stderr: stderr.trim() });
    });
  });
}

const results = [];
for (const script of scripts) {
  results.push(await run(script));
}

const errors = results
  .filter((result) => result.code !== 0)
  .map((result) => ({
    script: result.script,
    code: result.code,
    stderr: result.stderr,
    stdout: result.stdout
  }));

console.log(JSON.stringify({
  checks: {
    scripts: scripts.length,
    passed: results.length - errors.length,
    failed: errors.length
  },
  scripts: results.map((result) => ({
    script: result.script,
    code: result.code
  })),
  errors
}, null, 2));

if (errors.length > 0) process.exitCode = 1;
