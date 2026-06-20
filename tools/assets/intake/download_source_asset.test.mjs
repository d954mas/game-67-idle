import { strict as assert } from "node:assert";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import test from "node:test";

const execFileAsync = promisify(execFile);

test("download_source_asset writes incoming file, hash, and log", async () => {
  const root = await mkdtemp(join(tmpdir(), "asset-intake-"));
  try {
    const sourceFile = join(root, "source.txt");
    const library = join(root, "library");
    await writeFile(sourceFile, "asset payload");

    const { stdout } = await execFileAsync(process.execPath, [
      "tools/assets/intake/download_source_asset.mjs",
      "--url",
      sourceFile,
      "--source",
      "Example Source",
      "--slug",
      "Demo Asset",
      "--library",
      library,
      "--license-name",
      "CC0",
    ]);

    const record = JSON.parse(stdout);
    assert.equal(record.status, "incoming");
    assert.equal(record.source, "example-source");
    assert.equal(record.slug, "demo-asset");
    assert.equal(record.bytes, "asset payload".length);
    assert.equal(record.sha256.length, 64);

    const downloaded = await readFile(record.path, "utf8");
    assert.equal(downloaded, "asset payload");

    const log = await readFile(
      join(library, "_incoming", "example-source", "demo-asset", "download-log.md"),
      "utf8",
    );
    assert.match(log, /License checked before download: yes/);
    assert.match(log, /SHA256:/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});
