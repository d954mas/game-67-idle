import assert from 'node:assert/strict';
import {mkdtempSync,writeFileSync,readFileSync,mkdirSync,existsSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join,resolve,dirname} from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import test from 'node:test';
import {createHash} from 'node:crypto';
const feature=resolve(dirname(fileURLToPath(import.meta.url)),'..').replaceAll('\\','/');
function run(command,args,cwd){const r=spawnSync(command,args,{cwd,encoding:'utf8'});assert.equal(r.status,0,`${command} ${args.join(' ')}\n${r.error||''}\n${r.stdout}\n${r.stderr}`);return r.stdout;}
test('two consumers share one library and run isolated physics worlds',()=>{
 const dir=mkdtempSync(join(tmpdir(),'nt-box3d-'));
 writeFileSync(join(dir,'CMakeLists.txt'),`cmake_minimum_required(VERSION 3.25)\nproject(consumer VERSION 9.8.7 LANGUAGES C)\ninclude("${feature}/cmake/Box3D.cmake")\nadd_executable(proof "${feature}/tests/physics_smoke.c")\nbox3d_enable(proof)\nbox3d_enable(proof)\nadd_subdirectory(second)\nif(NOT PROJECT_VERSION STREQUAL "9.8.7")\n message(FATAL_ERROR "Feature changed consumer project version")\nendif()\nenable_testing()\nadd_test(NAME physics COMMAND proof)\nset_tests_properties(physics PROPERTIES LABELS core)\n`);
 mkdirSync(join(dir,'second'));writeFileSync(join(dir,'second/CMakeLists.txt'),`include("${feature}/cmake/Box3D.cmake")\nadd_library(second STATIC "${feature}/tests/physics_smoke.c")\nbox3d_enable(second)\n`);
 const compiler=process.env.CC || (process.platform==='win32'?'C:/Program Files/LLVM/bin/clang.exe':'cc');
 run('cmake',['-S',dir,'-B',join(dir,'build'),'-G','Ninja',`-DCMAKE_C_COMPILER=${compiler}`,'-DCMAKE_BUILD_TYPE=Debug'],dir);
 run('cmake',['--build',join(dir,'build')],dir);run('ctest',['--test-dir',join(dir,'build'),'--output-on-failure'],dir);
});

test('vendored distribution matches its pinned source integrity record',()=>{
 const manifest=JSON.parse(readFileSync(join(feature,'UPSTREAM.json'),'utf8'));
 assert.equal(manifest.license,'MIT');assert.equal(manifest.modifications.length,0);
 for(const row of manifest.files){
  const bytes=readFileSync(join(feature,'vendor/box3d',row.path));
  assert.equal(bytes.length,row.bytes,row.path);assert.equal(createHash('sha256').update(bytes).digest('hex'),row.sha256,row.path);
 }
});
