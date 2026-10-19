import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, writeFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { checkShaderSource, checkShaderPaths } from "../check_shaders.mjs";

const shader = (body) => `void main() { ${body} }`;

test("accepts local position calculations and ignores comments and lookalike names", () => {
  const source = `// gl_Position.z = 1.;\n/* gl_Position = vec4(0.); */\nvoid main(void) {
    vec4 my_gl_Position = vec4(1.); my_gl_Position.z = max(my_gl_Position.z, -my_gl_Position.w);
    gl_Position = my_gl_Position;
  }`;
  assert.deepEqual(checkShaderSource(source), []);
  assert.deepEqual(checkShaderSource("vec4 transform(vec4 p) { return p; }", "common.glsl"), []);
});

test("rejects the real shadow pancake and sky depth patterns", () => {
  for (const body of [
    "gl_Position = p; gl_Position.z = max(gl_Position.z, -gl_Position.w);",
    "gl_Position = clip.xyww; gl_Position.z -= 1.0e-6 * gl_Position.w;",
    "gl_Position = p; gl_Position = q;",
  ]) assert.ok(checkShaderSource(shader(body)).length, body);
});

test("rejects reads, swizzles, indices, compound assignments and inout escapes", () => {
  for (const body of ["gl_Position.z = 1.;", "gl_Position[2] = 1.;", "gl_Position += p;",
    "++gl_Position;", "gl_Position++;", "vec4 p = gl_Position;", "adjust(gl_Position);",
    "if (gl_Position == p) {}", "bool b = (gl_Position = p) == q;"])
    assert.ok(checkShaderSource(shader(body)).length, body);
});

test("does not accept repeated dynamic writes hidden in helpers, loops or macros", () => {
  for (const source of [
    "void write() { gl_Position = p; } void main() { write(); write(); }",
    shader("for (int i=0;i<2;i++) { gl_Position = p; }"),
    shader("for (int i=0;i<2;gl_Position=vec4(0.0),++i) {}"),
    shader("if (ready) gl_Position = p;"),
    "#define POS gl_Position\nvoid main() { POS = p; POS.z = 1.; }",
    "#define ENTRY void main() { gl_Position = p; }\nENTRY",
    "/* prefix */ #define ENTRY void main() { gl_Position = p; }\nENTRY",
    "#define ENTRY \\\nvoid main() { gl_Position = p; }\nENTRY",
  ]) assert.ok(checkShaderSource(source).length, source);
  assert.ok(checkShaderSource(shader("gl_Position = p;"), "shared.glsl").length);
});

test("diagnostics retain source lines across multiline comments and CRLF", () => {
  const result = checkShaderSource("/* one\r\n two */\r\nvoid main() {\r\n gl_Position.z = 1.;\r\n}", "x.vert");
  assert.equal(result[0].line, 4);
  assert.equal(result[0].file, "x.vert");
});

test("splices continued lines before comments and keeps original diagnostic locations", () => {
  const continuation = "\\" + "\r\n";
  const source = `void main() {\r\n gl_${continuation}Position.z = 1.;\r\n gl_Position.w = 1.;\r\n}`;
  assert.deepEqual(checkShaderSource(source).map((finding) => finding.line), [2, 4]);
  assert.deepEqual(checkShaderSource(`// ${continuation}gl_Position.z = 1.;\r\n${shader("gl_Position = p;")}`), []);
});

test("CLI checks explicit trees, skips generated output, and fails on missing input", () => {
  const root = mkdtempSync(join(tmpdir(), "shader-positions-"));
  try {
    writeFileSync(join(root, "ok.vert"), shader("gl_Position = vec4(1.);"));
    mkdirSync(join(root, "build"));
    writeFileSync(join(root, "build", "old.vert"), shader("gl_Position.z = 1.;"));
    assert.equal(checkShaderPaths([root, join(root, "ok.vert")]).files, 1);
    const cli = (args) => spawnSync(process.execPath, ["ai_studio/quality/check_shaders.mjs", ...args], { encoding: "utf8" });
    assert.equal(cli([root]).status, 0);
    writeFileSync(join(root, "bad.vert"), shader("gl_Position = p; gl_Position.z = 1.;"));
    const bad = cli(["--json", root]);
    assert.equal(bad.status, 1);
    assert.equal(JSON.parse(bad.stdout).diagnostics[0].rule, "QTECH_002");
    assert.equal(cli([join(root, "missing")]).status, 2);
    assert.equal(cli([]).status, 2);
    mkdirSync(join(root, "empty"));
    assert.equal(cli([join(root, "empty")]).status, 2);
  } finally { rmSync(root, { recursive: true, force: true }); }
});
