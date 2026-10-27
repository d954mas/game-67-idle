#!/usr/bin/env node
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, readdirSync, realpathSync, rmSync, writeFileSync } from 'node:fs';
import { dirname, extname, isAbsolute, join, relative, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { checkShaderPaths } from '../quality/check_shaders.mjs';

const hash = (source) => createHash('sha256').update(source).digest('hex');
const runCommand = (binary, args, options = {}) => {
  const result = spawnSync(binary, args, { encoding: 'utf8', maxBuffer: 4 * 1024 * 1024, ...options });
  return { status: result.status, stdout: result.stdout || '', stderr: result.stderr || '', error: result.error?.message };
};

function contained(root, file) {
  const path = realpathSync(file);
  const tail = relative(root, path);
  if (tail === '..' || tail.startsWith(`..\\`) || tail.startsWith('../') || isAbsolute(tail)) {
    throw new Error(`outside shader root: ${file}`);
  }
  return path;
}

function resolvesInside(root, target) {
  const absolute = resolve(target);
  let ancestor = absolute;
  while (!existsSync(ancestor)) {
    const parent = dirname(ancestor);
    if (parent === ancestor) throw new Error(`output ancestor not found: ${target}`);
    ancestor = parent;
  }
  const physical = resolve(realpathSync(ancestor), relative(ancestor, absolute));
  const tail = relative(root, physical);
  return !tail || (tail !== '..' && !tail.startsWith(`..\\`) && !tail.startsWith('../') && !isAbsolute(tail));
}

function writeOutput(file, content) {
  rmSync(file, { force: true });
  writeFileSync(file, content, { flag: 'wx' });
}

function engineLocations(root) {
  for (let dir = root; ; dir = dirname(dir)) {
    const header = join(dir, 'external', 'neotolis-engine', 'shared', 'include', 'nt_vertex_locations.h');
    if (existsSync(header)) {
      const definitions = [...readFileSync(header, 'utf8').matchAll(/^#define (NT_(?:LOC_|INST_LOC_)\w+)\s+(\d+)\b/gm)]
        .filter((match) => match[1] !== 'NT_LOC_VERTEX_END')
        .map((match) => `#define ${match[1]} ${match[2]}`);
      if (!definitions.some((line) => line.startsWith('#define NT_LOC_POSITION '))) {
        throw new Error(`engine locations header has no locations: ${header}`);
      }
      return { header: realpathSync(header), source: `#ifndef NT_LOC_POSITION\n${definitions.join('\n')}\n#endif\n` };
    }
    if (dir === dirname(dir)) break;
  }
  throw new Error('engine locations header not found for nt/locations.glsl');
}

export function prepareShader(shaderRoot, shaderFile) {
  const root = realpathSync(shaderRoot);
  const file = resolve(root, shaderFile);
  const requestedTail = relative(root, file);
  if (requestedTail === '..' || requestedTail.startsWith(`..\\`) || requestedTail.startsWith('../') || isAbsolute(requestedTail)) {
    throw new Error(`outside shader root: ${shaderFile}`);
  }
  if (!existsSync(file)) throw new Error(`shader not found: ${shaderFile}`);
  const dependencies = new Set();
  const once = new Set();
  const active = new Set();

  function expand(requested) {
    const path = contained(root, requested);
    if (once.has(path)) return '';
    if (active.has(path)) throw new Error(`shader include cycle: ${path}`);
    active.add(path);
    dependencies.add(path);
    const lines = readFileSync(path, 'utf8').split(/\r?\n/);
    if (lines.some((line) => /^\s*#pragma\s+once\b/.test(line))) once.add(path);
    const output = lines.map((line) => {
      if (/^\s*#pragma\s+once\b/.test(line)) return '';
      const match = line.match(/^\s*#\s*include\s+"([^"]+)"/);
      if (!match) return line;
      if (match[1] === 'nt/locations.glsl') {
        const locations = engineLocations(root);
        dependencies.add(locations.header);
        return locations.source;
      }
      const options = [resolve(dirname(path), match[1]), resolve(root, match[1])];
      const found = options.find((candidate) => existsSync(candidate));
      if (!found) throw new Error(`include not found: ${match[1]} in ${path}`);
      return expand(found);
    }).join('\n');
    active.delete(path);
    return output;
  }

  const body = expand(file);
  const versions = [...body.matchAll(/^\s*#\s*version\b/gm)];
  if (versions.length > 1 || (versions.length && body.slice(0, versions[0].index).trim())) {
    throw new Error(`invalid #version placement: ${shaderFile}`);
  }
  return { source: versions.length ? body : `#version 300 es\n${body}`, dependencies: [...dependencies] };
}

function resultOf(command) {
  return { status: command.error ? 'unavailable' : command.status === 0 ? 'pass' : 'fail',
    exitCode: command.status, output: [command.stdout, command.stderr, command.error].filter(Boolean).join('\n').trim() };
}

export function parseMaliMetrics(report) {
  const variants = [];
  let variant = 'main';
  for (const line of report.split(/\r?\n/)) {
    if (/^Position variant$/.test(line)) variant = 'position';
    else if (/^Varying variant$/.test(line)) variant = 'varying';
    const registers = line.match(/^Work registers:\s*(\d+).*?at\s*(\d+)% occupancy/);
    if (registers) variants.push({ variant, workRegisters: Number(registers[1]), occupancyPercent: Number(registers[2]) });
    const cycles = line.match(/^Total instruction cycles:\s*(.+)$/);
    if (cycles && variants.length) variants.at(-1).totalInstructionCycles = cycles[1].trim().replace(/\s+/g, ' ');
  }
  return variants;
}

function defaultMalioc() {
  if (process.env.MALIOC) return process.env.MALIOC;
  if (process.platform === 'win32') {
    const arm = 'C:/Program Files/Arm';
    if (existsSync(arm)) {
      const candidates = readdirSync(arm).filter((name) => name.startsWith('Arm Performance Studio ')).sort().reverse();
      for (const name of candidates) {
        const binary = join(arm, name, 'mali_offline_compiler', 'malioc.exe');
        if (existsSync(binary)) return binary;
      }
    }
  }
  return 'malioc';
}

function defaultPowerVRDir() {
  return process.env.POWERVR_COMPILER_DIR || join(fileURLToPath(new URL('../..', import.meta.url)), 'tmp', 'vendor', 'powervr', '2025_R2');
}

function powerVRMetrics(report) {
  const fields = Object.fromEntries([...report.matchAll(/^([A-Za-z]\w*)\s*=\s*([\d.]+)%?\s*$/gm)]
    .map((match) => [match[1], Number(match[2])]));
  return { instructionCount: fields.uInstructionCount, averageCycles: fields.uEmulatedCycleCountAverage,
    worstCycles: fields.uEmulatedCycleCountWorst, tempRegisters: fields.uTempRegisterCount,
    uscUtilisationPercent: fields.fUSCUtilisation };
}

function profilePowerVR({ out, vertexOut, fragmentOut, dir, bvncs, shaderRoot, run }) {
  if (!bvncs?.length) return { status: 'not_requested' };
  const compilerDir = resolve(dir);
  const compiler = join(compilerDir, 'ProfilingCompilerCLIWrapper.exe');
  if (!existsSync(compiler)) return { status: 'unavailable', reason: `PowerVR compiler not found: ${compiler}` };
  const targets = bvncs.map((bvnc) => {
    if (!/^\d+\.\d+\.\d+\.\d+$/.test(bvnc)) throw new Error(`invalid PowerVR BVNC: ${bvnc}`);
    const stages = [vertexOut, fragmentOut].map((source) => {
      const name = source.split(/[\\/]/).at(-1);
      const cwd = join(out, 'powervr', bvnc, name);
      if (resolvesInside(shaderRoot, cwd)) throw new Error('PowerVR output must be outside shader root');
      mkdirSync(cwd, { recursive: true });
      if (resolvesInside(shaderRoot, cwd)) throw new Error('PowerVR output must be outside shader root');
      const prof = join(cwd, `${name}.prof`);
      const asm = join(cwd, `${name}.asm`);
      rmSync(prof, { force: true });
      rmSync(asm, { force: true });
      const result = resultOf(run(compiler, ['-a', 'glsles', '-b', bvnc, '-i', resolve(source), '-p', compilerDir, '-x'], { cwd }));
      writeOutput(join(cwd, 'compiler.stdout.txt'), `${result.output}\n`);
      const metrics = existsSync(prof) ? powerVRMetrics(readFileSync(prof, 'utf8')) : null;
      const passed = result.status === 'pass' && /RESULT:\s*Success\b/i.test(result.output)
        && Number.isFinite(metrics?.instructionCount);
      return { file: name, status: result.status === 'unavailable' ? 'unavailable' : passed ? 'pass' : 'fail',
        metrics: passed ? metrics : null,
        reportDir: relative(out, cwd).replaceAll('\\', '/') };
    });
    return { bvnc, status: stages.every((stage) => stage.status === 'pass') ? 'pass'
      : stages.some((stage) => stage.status === 'unavailable') ? 'unavailable' : 'fail', stages };
  });
  return { status: targets.every((target) => target.status === 'pass') ? 'pass'
    : targets.some((target) => target.status === 'unavailable') ? 'unavailable' : 'fail', targets };
}

export function analyzePair({ root, vertex, fragment, out, glslang = 'glslangValidator', malioc, maliGpu,
  powervrDir = defaultPowerVRDir(), powervrBvncs, run = runCommand }) {
  if (!root || !vertex || !fragment || !out) throw new Error('root, vertex, fragment and out are required');
  if (extname(vertex) !== '.vert' || extname(fragment) !== '.frag') throw new Error('expected .vert and .frag files');
  const shaderRoot = realpathSync(root);
  if (resolvesInside(shaderRoot, out)) {
    throw new Error('output must be outside shader root');
  }
  const vs = prepareShader(root, vertex);
  const fs = prepareShader(root, fragment);
  const rulePaths = [resolve(root, vertex), ...vs.dependencies, ...fs.dependencies]
    .filter((path) => ['.vert', '.glsl'].includes(extname(path)));
  const ruleFindings = checkShaderPaths([...new Set(rulePaths)]).diagnostics;
  mkdirSync(out, { recursive: true });
  if (resolvesInside(shaderRoot, out)) throw new Error('output must be outside shader root');
  const vertexOut = join(out, 'program.vert');
  const fragmentOut = join(out, 'program.frag');
  writeOutput(vertexOut, vs.source);
  writeOutput(fragmentOut, fs.source);

  const referenceRun = resultOf(run(glslang, ['-l', vertexOut, fragmentOut]));
  writeOutput(join(out, 'glslang.txt'), `${referenceRun.output}\n`);
  const reference = { status: referenceRun.status, exitCode: referenceRun.exitCode, reportFile: 'glslang.txt' };
  let mali = { status: 'not_requested' };
  if (maliGpu) {
    const stages = [vertexOut, fragmentOut].map((file) => {
      const value = resultOf(run(malioc || 'malioc', ['-c', maliGpu, file]));
      writeOutput(`${file}.mali.txt`, `${value.output}\n`);
      const name = file.split(/[\\/]/).at(-1);
      return { file: name, status: value.status, exitCode: value.exitCode,
        metrics: parseMaliMetrics(value.output), reportFile: `${name}.mali.txt` };
    });
    mali = { status: stages.every((stage) => stage.status === 'pass') ? 'pass'
      : stages.some((stage) => stage.status === 'unavailable') ? 'unavailable' : 'fail', gpu: maliGpu, stages };
  }

  const powerVR = profilePowerVR({ out, vertexOut, fragmentOut, dir: powervrDir,
    bvncs: powervrBvncs, shaderRoot, run });
  const report = {
    schema: 'ai_studio.shader_analysis.v1',
    input: { root: realpathSync(root), vertex, fragment },
    prepared: { vertexSha256: hash(vs.source), fragmentSha256: hash(fs.source),
      vertexDependencies: vs.dependencies, fragmentDependencies: fs.dependencies },
    ruleFindings, reference, mali, powerVR,
  };
  writeOutput(join(out, 'report.json'), `${JSON.stringify(report, null, 2)}\n`);
  return report;
}

function parseOptions(args) {
  const options = {};
  for (let i = 0; i < args.length; i += 2) {
    if (!args[i]?.startsWith('--') || !args[i + 1] || args[i + 1].startsWith('--')) throw new Error(`expected --key value: ${args[i] || ''}`);
    const name = args[i].slice(2).replaceAll('-', '');
    if (!['root', 'vertex', 'fragment', 'out', 'glslang', 'malioc', 'maligpu', 'powervrbvnc', 'powervrdir'].includes(name) || options[name]) {
      throw new Error(`unknown or repeated option: ${args[i]}`);
    }
    options[name] = args[i + 1];
  }
  return options;
}

export function main(args = process.argv.slice(2)) {
  try {
    if (args[0] === 'doctor' && args.length === 1) {
      const glslang = resultOf(runCommand('glslangValidator', ['-v']));
      const mali = resultOf(runCommand(defaultMalioc(), ['--help']));
      const powerVR = existsSync(join(defaultPowerVRDir(), 'ProfilingCompilerCLIWrapper.exe')) ? 'available' : 'unavailable';
      console.log(JSON.stringify({ glslang: glslang.status, mali: mali.status,
        powerVR,
        shaderRule: 'ai_studio/quality/check_shaders.mjs' }, null, 2));
      return 0;
    }
    if (args[0] === 'rules') {
      const options = parseOptions(args.slice(1));
      if (!options.root || Object.keys(options).length !== 1) throw new Error('usage: node ai_studio/shaders/cli.mjs rules --root DIR');
      const result = checkShaderPaths([options.root]);
      console.log(JSON.stringify({ files: result.files, findings: result.diagnostics.length, diagnostics: result.diagnostics }));
      return result.diagnostics.length ? 1 : 0;
    }
    if (args[0] !== 'analyze') throw new Error('usage: node ai_studio/shaders/cli.mjs doctor | rules --root DIR | analyze --root DIR --vertex FILE.vert --fragment FILE.frag --out DIR [--mali-gpu GPU] [--powervr-bvnc BVNC[,BVNC]]');
    const options = parseOptions(args.slice(1));
    const result = analyzePair({ root: options.root, vertex: options.vertex, fragment: options.fragment,
      out: options.out, glslang: options.glslang, maliGpu: options.maligpu,
      malioc: options.malioc || defaultMalioc(), powervrDir: options.powervrdir || defaultPowerVRDir(),
      powervrBvncs: options.powervrbvnc?.split(',').map((value) => value.trim()) });
    console.log(JSON.stringify({ output: resolve(options.out), reference: result.reference.status,
      ruleFindings: result.ruleFindings.length, mali: result.mali.status, powerVR: result.powerVR.status }));
    return [result.reference, result.mali, result.powerVR].some((value) => value.status === 'unavailable') ? 2
      : [result.reference, result.mali, result.powerVR].some((value) => value.status === 'fail') || result.ruleFindings.length ? 1 : 0;
  } catch (error) {
    console.error(`shader analysis: ${error.message}`);
    return 2;
  }
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) process.exitCode = main();
