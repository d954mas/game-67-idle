import test from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, linkSync, mkdtempSync, mkdirSync, readFileSync, rmSync, symlinkSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, relative } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { analyzePair, parseMaliMetrics, prepareShader } from './cli.mjs';

function fixture() {
  const root = mkdtempSync(join(tmpdir(), 'studio-shaders-'));
  mkdirSync(join(root, 'common'));
  writeFileSync(join(root, 'common', 'shared.glsl'), '#pragma once\nvec4 position() { return vec4(0.0); }\n');
  writeFileSync(join(root, 'demo.vert'), '#include "common/shared.glsl"\n#include "common/shared.glsl"\nvoid main() { gl_Position = position(); }\n');
  writeFileSync(join(root, 'demo.frag'), 'precision mediump float;\nout vec4 color;\nvoid main() { color = vec4(1.0); }\n');
  return root;
}

test('prepares ESSL 300 from builder-style includes without duplicating pragma-once files', () => {
  const root = fixture();
  try {
    const result = prepareShader(root, 'demo.vert');
    assert.match(result.source, /^#version 300 es\n/);
    assert.equal(result.source.match(/vec4 position\(/g)?.length, 1);
    assert.doesNotMatch(result.source, /#include|#pragma once/);
    assert.equal(result.dependencies.length, 2);
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test('missing includes and paths outside shader root fail before a compiler can pass', () => {
  const root = fixture();
  try {
    writeFileSync(join(root, 'broken.vert'), '#include "missing.glsl"\nvoid main() {}\n');
    assert.throws(() => prepareShader(root, 'broken.vert'), /include not found/);
    assert.throws(() => prepareShader(root, '../other.vert'), /outside shader root/);
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test('engine virtual locations include uses the pinned engine header', () => {
  const root = fixture();
  try {
    const headerDir = join(root, 'external', 'neotolis-engine', 'shared', 'include');
    mkdirSync(headerDir, { recursive: true });
    writeFileSync(join(headerDir, 'nt_vertex_locations.h'), '#define NT_LOC_POSITION 0\n#define NT_INST_LOC_WORLD_ROW0 8\n');
    writeFileSync(join(root, 'locations.vert'), '#include "nt/locations.glsl"\nlayout(location = NT_LOC_POSITION) in vec3 p;\nvoid main() { gl_Position = vec4(p, 1.0); }\n');
    const result = prepareShader(root, 'locations.vert');
    assert.match(result.source, /#define NT_LOC_POSITION 0/);
    assert.match(result.source, /#define NT_INST_LOC_WORLD_ROW0 8/);
    assert.equal(result.dependencies.length, 2);
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test('one analysis reports reference link and optional vendor metrics separately', () => {
  const root = fixture();
  const out = mkdtempSync(join(tmpdir(), 'studio-shaders-out-'));
  const calls = [];
  const run = (binary, args) => {
    calls.push({ binary, args });
    return { status: 0, stdout: binary === 'malioc' ? 'Work registers: 31\n' : '', stderr: '' };
  };
  try {
    const result = analyzePair({ root, vertex: 'demo.vert', fragment: 'demo.frag', out, glslang: 'glslangValidator', malioc: 'malioc', maliGpu: 'Mali-G76', run });
    assert.equal(result.reference.status, 'pass');
    assert.equal(result.mali.status, 'pass');
    assert.equal(result.ruleFindings.length, 0);
    assert.deepEqual(calls.map(({ args }) => args[0]), ['-l', '-c', '-c']);
    assert.match(readFileSync(join(out, 'program.vert'), 'utf8'), /^#version 300 es/);
    assert.equal(JSON.parse(readFileSync(join(out, 'report.json'), 'utf8')).mali.gpu, 'Mali-G76');
  } finally {
    rmSync(root, { recursive: true, force: true });
    rmSync(out, { recursive: true, force: true });
  }
});

test('analysis output cannot be written into the game shader source tree', () => {
  const root = fixture();
  try {
    assert.throws(() => analyzePair({ root, vertex: 'demo.vert', fragment: 'demo.frag', out: join(root, 'generated') }), /outside shader root/);
  } finally { rmSync(root, { recursive: true, force: true }); }
});

test('analysis rejects an output junction into the shader source tree', (t) => {
  const root = fixture();
  const outside = mkdtempSync(join(tmpdir(), 'studio-shaders-output-'));
  const link = join(outside, 'linked-source');
  try {
    try { symlinkSync(root, link, process.platform === 'win32' ? 'junction' : 'dir'); }
    catch (error) {
      if (['EPERM', 'EACCES'].includes(error.code)) return t.skip('symlink creation is unavailable');
      throw error;
    }
    assert.throws(() => analyzePair({ root, vertex: 'demo.vert', fragment: 'demo.frag', out: join(link, 'generated') }), /outside shader root/);
    assert.equal(existsSync(join(root, 'generated')), false);
  } finally {
    rmSync(outside, { recursive: true, force: true });
    rmSync(root, { recursive: true, force: true });
  }
});

test('existing output hard links do not overwrite shader sources', (t) => {
  const root = fixture();
  const out = mkdtempSync(join(tmpdir(), 'studio-shaders-output-'));
  const source = join(root, 'demo.vert');
  const before = readFileSync(source, 'utf8');
  try {
    try { linkSync(source, join(out, 'program.vert')); }
    catch (error) {
      if (['EPERM', 'EACCES', 'EXDEV'].includes(error.code)) return t.skip('hard links are unavailable');
      throw error;
    }
    analyzePair({ root, vertex: 'demo.vert', fragment: 'demo.frag', out,
      run: () => ({ status: 0, stdout: '', stderr: '' }) });
    assert.equal(readFileSync(source, 'utf8'), before);
    assert.match(readFileSync(join(out, 'program.vert'), 'utf8'), /^#version 300 es/);
  } finally {
    rmSync(out, { recursive: true, force: true });
    rmSync(root, { recursive: true, force: true });
  }
});

test('PowerVR profiles record real vendor instruction counts per stage and reject missing output', () => {
  const root = fixture();
  const out = mkdtempSync(join(tmpdir(), 'studio-powervr-out-'));
  const vendor = mkdtempSync(join(tmpdir(), 'studio-powervr-vendor-'));
  writeFileSync(join(vendor, 'ProfilingCompilerCLIWrapper.exe'), 'fixture');
  const run = (binary, args, options) => {
    if (!binary.endsWith('ProfilingCompilerCLIWrapper.exe')) return { status: 0, stdout: '', stderr: '' };
    assert.equal(binary, join(vendor, 'ProfilingCompilerCLIWrapper.exe'));
    const source = args[args.indexOf('-i') + 1];
    writeFileSync(join(options.cwd, `${source.split(/[\\/]/).at(-1)}.prof`), 'uInstructionCount = 22\nuEmulatedCycleCountAverage = 39\n');
    return { status: 0, stdout: 'RESULT:\n Success', stderr: '' };
  };
  try {
    const result = analyzePair({ root, vertex: 'demo.vert', fragment: 'demo.frag', out,
      powervrDir: relative(process.cwd(), vendor), powervrBvncs: ['22.87.104.18'], run });
    assert.equal(result.powerVR.status, 'pass');
    assert.equal(result.powerVR.targets[0].stages[0].metrics.instructionCount, 22);
    assert.equal(result.powerVR.targets[0].stages[1].metrics.averageCycles, 39);
    const missing = analyzePair({ root, vertex: 'demo.vert', fragment: 'demo.frag', out: join(out, 'missing'),
      powervrDir: vendor, powervrBvncs: ['35.3.408.101'],
      run: (binary) => ({ status: 0, stdout: binary.endsWith('ProfilingCompilerCLIWrapper.exe') ? 'RESULT:\n Success' : '', stderr: '' }) });
    assert.equal(missing.powerVR.status, 'fail');
    const malformed = analyzePair({ root, vertex: 'demo.vert', fragment: 'demo.frag', out: join(out, 'malformed'),
      powervrDir: vendor, powervrBvncs: ['35.3.408.101'],
      run: (binary, args, options) => {
        if (binary.endsWith('ProfilingCompilerCLIWrapper.exe')) {
          const source = args[args.indexOf('-i') + 1];
          writeFileSync(join(options.cwd, `${source.split(/[\\/]/).at(-1)}.prof`), 'unrecognized report\n');
        }
        return { status: 0, stdout: 'RESULT:\n Success', stderr: '' };
      } });
    assert.equal(malformed.powerVR.status, 'fail');
  } finally {
    rmSync(root, { recursive: true, force: true });
    rmSync(out, { recursive: true, force: true });
    rmSync(vendor, { recursive: true, force: true });
  }
});

test('Mali metrics keep register and cycle estimates distinct from instruction counts', () => {
  const metrics = parseMaliMetrics('Position variant\nWork registers: 32 (100% used at 100% occupancy)\nTotal instruction cycles: 0.54 1.00 0.00 LS\nVarying variant\nWork registers: 24 (75% used at 100% occupancy)\nTotal instruction cycles: 0.21 12.00 0.00 LS');
  assert.deepEqual(metrics, [
    { variant: 'position', workRegisters: 32, occupancyPercent: 100, totalInstructionCycles: '0.54 1.00 0.00 LS' },
    { variant: 'varying', workRegisters: 24, occupancyPercent: 100, totalInstructionCycles: '0.21 12.00 0.00 LS' },
  ]);
});

test('shared entry point scans an entire shader directory for writing-rule violations', () => {
  const root = fixture();
  const cli = fileURLToPath(new URL('./cli.mjs', import.meta.url));
  try {
    const clean = spawnSync(process.execPath, [cli, 'rules', '--root', root], { encoding: 'utf8' });
    assert.equal(clean.status, 0);
    writeFileSync(join(root, 'bad.vert'), 'void main() { gl_Position = vec4(1.0); gl_Position.z = 0.0; }');
    const bad = spawnSync(process.execPath, [cli, 'rules', '--root', root], { encoding: 'utf8' });
    assert.equal(bad.status, 1);
    assert.ok(JSON.parse(bad.stdout).findings > 0);
  } finally { rmSync(root, { recursive: true, force: true }); }
});
