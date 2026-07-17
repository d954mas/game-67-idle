import assert from "node:assert/strict";
import { spawn } from "node:child_process";
import { once } from "node:events";
import { copyFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, symlinkSync, utimesSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { addImage, createProject, getProject, updateProject } from "../ops.mjs";
import { scanPackManifestSource } from "../../manifests/ops.mjs";
import { thresholdsFromStyleLock } from "../../tools/image/quality_gate/api.mjs";
import { __promoteAssetToGameForTest, __withPromotionLockForTest } from "../ops/asset_promotion.mjs";
import { solidPng } from "./png_fixture.mjs";

function acceptedProject(root) {
  const project = createProject(root, {
    title: "Promote accepted art",
    ownership: { kind: "game", gameId: "demo-game" },
  });
  const target = addImage(root, project.id, { name: "hero portrait.png", bytes: solidPng(64, 64, [120, 80, 40]) }).element;
  const world = addImage(root, project.id, { name: "world.png", bytes: solidPng(64, 64, [40, 80, 120]) }).element;
  const gui = addImage(root, project.id, { name: "gui.png", bytes: solidPng(64, 64, [80, 40, 120]) }).element;
  const lock = {
    id: "demo-style-v1",
    bg_rule: { mode: "chroma", key_color: "#FF00FF" },
    asset_size: { width: 64, height: 64 },
    technical_gate: {
      max_spill_edge_ratio: 0.05,
      max_halo_edge_ratio: 0.04,
      max_alpha_noise_ratio: 0.03,
      max_empty_margin_ratio: 0.5,
      max_aspect_relative_error: 0.02,
    },
    prompt_preamble: "Chunky readable forms.",
    negative_prompt: "No photorealism.",
    exemplar_refs: [
      { ref: `canvas://${project.id}/element/${world.id}`, origin: "owned", domain: "world" },
      { ref: `canvas://${project.id}/element/${gui.id}`, origin: "owned", domain: "gui" },
    ],
  };
  const checkedAt = "2026-07-17T00:00:00.000Z";
  const decidedAt = "2026-07-17T00:01:00.000Z";
  const current = getProject(root, project.id);
  updateProject(root, project.id, {
    elements: current.elements.map((element) => element.id === target.id ? {
      ...element,
      assetStatus: "accepted",
      meta: {
        ...(element.meta || {}),
        technical_gate: {
          schema: "game.asset_technical_gate",
          version: 1,
          verdict: "pass",
          style_lock_id: lock.id,
          source_ref: target.src,
          key_color: lock.bg_rule.key_color,
          thresholds: thresholdsFromStyleLock(lock),
        },
        style_verdict: {
          schema: "game.asset_style_verdict",
          version: 1,
          verdict: "reject",
          checked_at: checkedAt,
          style_lock_id: lock.id,
          style_lock_snapshot: structuredClone(lock),
          source_ref: target.src,
          exemplar_refs: lock.exemplar_refs.map((entry, index) => ({
            ...entry,
            source_ref: index === 0 ? world.src : gui.src,
          })),
          do_prompt: lock.prompt_preamble,
          dont_prompt: lock.negative_prompt,
        },
        style_decision: {
          schema: "game.asset_style_decision",
          version: 1,
          decision: "accept",
          decided_at: decidedAt,
          reason: "Lead accepts the deliberate exception.",
          style_lock_id: lock.id,
          source_ref: target.src,
          advisory_verdict: "reject",
          advisory_checked_at: checkedAt,
        },
      },
    } : element),
  });
  return { projectId: project.id, target, lock };
}

function fixture(t, { privateStore = false } = {}) {
  const root = mkdtempSync(join(tmpdir(), "canvas-asset-promotion-"));
  const projectsRoot = join(root, "canvas-projects");
  mkdirSync(join(root, "games", ...(privateStore ? ["private"] : []), "demo-game", "assets"), { recursive: true });
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = projectsRoot;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(root, { recursive: true, force: true });
  });
  return { root, ...acceptedProject(root) };
}

const METADATA = {
  asset_id: "demo__hero_portrait__cc0-1-0",
  title: "Hero Portrait",
  description: "Accepted Canvas portrait.",
  kind: "ui",
  origin: "ai",
  license: "CC0-1.0",
  license_url: "https://creativecommons.org/publicdomain/zero/1.0/",
  license_kind: "cc",
  attribution_required: "false",
  notice_required: "false",
  credit_text: "",
  commercial_use: "true",
  modification_allowed: "true",
  redistribution_allowed: "true",
  publish: "true",
  source_page: "canvas://demo-game/hero-portrait",
  author_vendor: "AI Studio Canvas",
  provenance: "Generated and reviewed in the game-owned Canvas workflow.",
  tags: ["hero", "portrait"],
};

function dependencies(fixture) {
  return {
    resolveStyleLock() {
      return { gameId: "demo-game", lock: fixture.lock };
    },
  };
}

async function crashPromotion(fx, mode, metadata = METADATA) {
  const metadataPath = join(fx.root, `${mode}-metadata.json`);
  const lockPath = join(fx.root, `${mode}-style-lock.json`);
  writeFileSync(metadataPath, JSON.stringify(metadata));
  writeFileSync(lockPath, JSON.stringify(fx.lock));
  const helper = fileURLToPath(new URL("./promotion_process_fixture.mjs", import.meta.url));
  const child = spawn(process.execPath, [
    helper,
    mode,
    fx.root,
    fx.projectId,
    fx.target.id,
    metadataPath,
    lockPath,
  ], { env: { ...process.env }, stdio: ["ignore", "pipe", "pipe"] });
  return (await once(child, "exit"))[0];
}

test("accepted Canvas art promotes into a game-local Pack Manifest with integrity and decision provenance", async (t) => {
  const fx = fixture(t);
  const result = await __promoteAssetToGameForTest(fx.root, {
    projectId: fx.projectId,
    elementId: fx.target.id,
    metadata: METADATA,
  }, dependencies(fx));

  assert.equal(result.record.asset_id, METADATA.asset_id);
  assert.equal(result.record.origin, "ai");
  assert.equal(result.record.classification, "product-asset");
  assert.match(result.record.sha256, /^[0-9a-f]{64}$/);
  assert.ok(result.record.bytes > 0);
  assert.equal(result.record.canvas_ref, `canvas://${fx.projectId}/element/${fx.target.id}`);
  assert.equal(result.record.style_decision.decision, "accept");
  assert.equal(result.record.style_decision.advisory_verdict, "reject");
  assert.match(result.record.resource, /^files\/demo__hero_portrait__cc0-1-0\/.+\.png$/);
  assert.deepEqual(readFileSync(result.destination), readFileSync(result.source));

  const rows = readFileSync(result.manifestPath, "utf8").trim().split(/\r?\n/).map((line) => JSON.parse(line));
  assert.deepEqual(rows, [result.record]);
  const second = await __promoteAssetToGameForTest(fx.root, {
    projectId: fx.projectId,
    elementId: fx.target.id,
    metadata: { ...METADATA, asset_id: "demo__hero_portrait_alt__cc0-1-0", title: "Hero Portrait Alt" },
  }, dependencies(fx));
  const twoRows = readFileSync(result.manifestPath, "utf8").trim().split(/\r?\n/).map((line) => JSON.parse(line));
  assert.deepEqual(twoRows.map((row) => row.asset_id), [result.record.asset_id, second.record.asset_id]);
  const scanned = await scanPackManifestSource(join(fx.root, "games", "demo-game", "assets"));
  assert.deepEqual(scanned.records.map((row) => row.asset_id).sort(), twoRows.map((row) => row.asset_id).sort());
  await assert.rejects(
    () => __promoteAssetToGameForTest(fx.root, {
      projectId: fx.projectId,
      elementId: fx.target.id,
      metadata: METADATA,
    }, dependencies(fx)),
    /already exists/,
  );
});

test("private-game promotion records the canonical scoped Canvas provenance ref", async (t) => {
  const fx = fixture(t, { privateStore: true });
  const result = await __promoteAssetToGameForTest(fx.root, {
    projectId: fx.projectId,
    elementId: fx.target.id,
    metadata: METADATA,
  }, dependencies(fx));
  assert.equal(result.record.canvas_ref, `canvas://game/demo-game/${fx.projectId}/element/${fx.target.id}`);
  assert.match(result.destination.replaceAll("\\", "/"), /games\/private\/demo-game\/assets\/packs\/canvas-promotions/);
});

test("promotion refuses non-accepted, forged, stale, and non-publishable assets before writing", async (t) => {
  const fx = fixture(t);
  const args = { projectId: fx.projectId, elementId: fx.target.id, metadata: METADATA };
  const accepted = getProject(fx.root, fx.projectId);

  updateProject(fx.root, fx.projectId, {
    elements: accepted.elements.map((element) => element.id === fx.target.id ? { ...element, assetStatus: "checked" } : element),
  });
  await assert.rejects(() => __promoteAssetToGameForTest(fx.root, args, dependencies(fx)), /requires accepted/);

  updateProject(fx.root, fx.projectId, {
    elements: accepted.elements.map((element) => element.id === fx.target.id ? {
      ...element,
      meta: { ...element.meta, style_decision: undefined },
    } : element),
  });
  await assert.rejects(() => __promoteAssetToGameForTest(fx.root, args, dependencies(fx)), /explicit current lead accept decision/);

  updateProject(fx.root, fx.projectId, {
    elements: accepted.elements.map((element) => element.id === fx.target.id ? {
      ...element,
      meta: { ...element.meta, style_decision: { ...element.meta.style_decision, source_ref: "files/stale.png" } },
    } : element),
  });
  await assert.rejects(() => __promoteAssetToGameForTest(fx.root, args, dependencies(fx)), /explicit current lead accept decision/);

  updateProject(fx.root, fx.projectId, { elements: accepted.elements });
  await assert.rejects(
    () => __promoteAssetToGameForTest(fx.root, {
      ...args,
      metadata: { ...METADATA, license: "Unknown", license_kind: "unknown", publish: "false" },
    }, dependencies(fx)),
    /metadata\.license must be resolved/,
  );
});

test("promotion rejects release-incomplete license and provenance metadata", async (t) => {
  const fx = fixture(t);
  const args = { projectId: fx.projectId, elementId: fx.target.id };
  const invalid = [
    [{ ...METADATA, license_kind: "unknown" }, /private\/unknown assets/],
    [{ ...METADATA, license_kind: "private" }, /private\/unknown assets/],
    [{ ...METADATA, provenance: "pending source audit" }, /must be resolved/],
    [{ ...METADATA, source_page: "unknown" }, /must be resolved/],
    [{ ...METADATA, license: "CC-BY-4.0", license_url: "https://creativecommons.org/licenses/by/4.0/", attribution_required: "true", notice_required: "true", author_vendor: "unknown", credit_text: "" }, /metadata\.author_vendor must be resolved/],
  ];
  for (const [metadata, expected] of invalid) {
    await assert.rejects(
      () => __promoteAssetToGameForTest(fx.root, { ...args, metadata }, dependencies(fx)),
      expected,
    );
  }
});

test("promotion rejects junction escapes and changed content-addressed source bytes", async (t) => {
  const fx = fixture(t);
  const args = { projectId: fx.projectId, elementId: fx.target.id, metadata: METADATA };
  writeFileSync(join(process.env.CANVAS_PROJECTS_ROOT, fx.projectId, fx.target.src), Buffer.from("changed pixels"));
  await assert.rejects(
    () => __promoteAssetToGameForTest(fx.root, args, dependencies(fx)),
    /no longer match the accepted content-addressed Canvas reference/,
  );

  const safe = fixture(t);
  const outside = mkdtempSync(join(tmpdir(), "canvas-asset-promotion-outside-"));
  t.after(() => rmSync(outside, { recursive: true, force: true }));
  const assets = join(safe.root, "games", "demo-game", "assets");
  rmSync(assets, { recursive: true, force: true });
  symlinkSync(outside, assets, process.platform === "win32" ? "junction" : "dir");
  await assert.rejects(
    () => __promoteAssetToGameForTest(safe.root, {
      projectId: safe.projectId,
      elementId: safe.target.id,
      metadata: METADATA,
    }, dependencies(safe)),
    /physical directory, not a symlink or junction/,
  );
  assert.equal(existsSync(join(outside, "packs")), false);

  const linkedSource = fixture(t);
  const outsideFiles = mkdtempSync(join(tmpdir(), "canvas-asset-promotion-source-"));
  t.after(() => rmSync(outsideFiles, { recursive: true, force: true }));
  const projectFiles = join(process.env.CANVAS_PROJECTS_ROOT, linkedSource.projectId, "files");
  copyFileSync(join(projectFiles, linkedSource.target.src.replace(/^files[\\/]/, "")), join(outsideFiles, linkedSource.target.src.replace(/^files[\\/]/, "")));
  rmSync(projectFiles, { recursive: true, force: true });
  symlinkSync(outsideFiles, projectFiles, process.platform === "win32" ? "junction" : "dir");
  await assert.rejects(
    () => __promoteAssetToGameForTest(linkedSource.root, {
      projectId: linkedSource.projectId,
      elementId: linkedSource.target.id,
      metadata: METADATA,
    }, dependencies(linkedSource)),
    /Canvas project files directory must be a physical directory/,
  );
});

test("same-game promotion lock serializes concurrent writers and rollback leaves no partial pack", async (t) => {
  const fx = fixture(t);
  const other = { root: fx.root, ...acceptedProject(fx.root) };
  let active = 0;
  let maxActive = 0;
  const insideLock = async () => {
    active += 1;
    maxActive = Math.max(maxActive, active);
    await new Promise((resolveWait) => setTimeout(resolveWait, 40));
    active -= 1;
  };
  const first = __promoteAssetToGameForTest(fx.root, {
    projectId: fx.projectId,
    elementId: fx.target.id,
    metadata: METADATA,
  }, { ...dependencies(fx), insideLock });
  const second = __promoteAssetToGameForTest(fx.root, {
    projectId: other.projectId,
    elementId: other.target.id,
    metadata: { ...METADATA, asset_id: "demo__hero_portrait_concurrent__cc0-1-0" },
  }, { ...dependencies(other), insideLock });
  const results = await Promise.all([first, second]);
  assert.equal(maxActive, 1);
  const rows = readFileSync(results[0].manifestPath, "utf8").trim().split(/\r?\n/).map(JSON.parse);
  assert.equal(rows.length, 2);

  const failed = fixture(t);
  await assert.rejects(
    () => __promoteAssetToGameForTest(failed.root, {
      projectId: failed.projectId,
      elementId: failed.target.id,
      metadata: METADATA,
    }, {
      ...dependencies(failed),
      beforeCommit() { throw new Error("injected commit failure"); },
    }),
    /injected commit failure/,
  );
  assert.equal(existsSync(join(failed.root, "games", "demo-game", "assets", "packs", "canvas-promotions")), false);
});

test("cross-process game lock keeps a live holder past the stale threshold", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "canvas-asset-promotion-lock-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const helper = fileURLToPath(new URL("./promotion_process_fixture.mjs", import.meta.url));
  const env = { ...process.env, CANVAS_LOCAL_CACHE_ROOT: join(root, "cache") };
  const first = spawn(process.execPath, [helper, "lock", root, "demo-game", "500", "100"], { env, stdio: ["ignore", "pipe", "pipe"] });
  let firstOutput = "";
  first.stdout.setEncoding("utf8");
  first.stdout.on("data", (chunk) => { firstOutput += chunk; });
  while (!firstOutput.includes("acquired")) await new Promise((resolveWait) => setTimeout(resolveWait, 10));

  const second = spawn(process.execPath, [helper, "lock", root, "demo-game", "0", "100"], { env, stdio: ["ignore", "pipe", "pipe"] });
  let secondOutput = "";
  second.stdout.setEncoding("utf8");
  second.stdout.on("data", (chunk) => { secondOutput += chunk; });
  const [[firstCode], [secondCode]] = await Promise.all([once(first, "exit"), once(second, "exit")]);
  assert.equal(firstCode, 0);
  assert.equal(secondCode, 0);
  const firstRelease = Number(/released (\d+)/.exec(firstOutput)?.[1]);
  const secondAcquire = Number(/acquired (\d+)/.exec(secondOutput)?.[1]);
  assert.ok(secondAcquire >= firstRelease, `${secondOutput} must acquire after ${firstOutput}`);
});

test("promotion lock recovers an abandoned zero-byte lock by file age", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "canvas-asset-promotion-torn-lock-"));
  const previous = process.env.CANVAS_LOCAL_CACHE_ROOT;
  process.env.CANVAS_LOCAL_CACHE_ROOT = join(root, "cache");
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_LOCAL_CACHE_ROOT;
    else process.env.CANVAS_LOCAL_CACHE_ROOT = previous;
    rmSync(root, { recursive: true, force: true });
  });
  const lockDir = join(process.env.CANVAS_LOCAL_CACHE_ROOT, "asset-promotion-locks");
  const lockPath = join(lockDir, "demo-game.lock");
  mkdirSync(lockDir, { recursive: true });
  writeFileSync(lockPath, "");
  const old = new Date(Date.now() - 1000);
  utimesSync(lockPath, old, old);
  const result = await __withPromotionLockForTest(root, "demo-game", async () => "recovered", {
    staleMs: 50,
    retryTotalMs: 500,
    retryIntervalMs: 10,
  });
  assert.equal(result, "recovered");
  assert.equal(existsSync(lockPath), false);
});

test("next promotion recovers a child-process crash after destination commit", async (t) => {
  const fx = fixture(t);
  const code = await crashPromotion(fx, "crash-after-destination");
  assert.equal(code, 91);

  const result = await __promoteAssetToGameForTest(fx.root, {
    projectId: fx.projectId,
    elementId: fx.target.id,
    metadata: METADATA,
  }, dependencies(fx));
  const rows = readFileSync(result.manifestPath, "utf8").trim().split(/\r?\n/).map(JSON.parse);
  assert.deepEqual(rows.map((row) => row.asset_id), [METADATA.asset_id]);
  assert.equal(existsSync(join(dirname(result.manifestPath), ".asset-promotion-transaction.prepared.json")), false);
});

test("committed marker remains authoritative if cleanup process dies", async (t) => {
  const fx = fixture(t);
  const code = await crashPromotion(fx, "crash-after-prepared-cleanup");
  assert.equal(code, 92);
  const packDir = join(fx.root, "games", "demo-game", "assets", "packs", "canvas-promotions");
  const committedRows = readFileSync(join(packDir, "assets.jsonl"), "utf8").trim().split(/\r?\n/).map(JSON.parse);
  assert.equal(committedRows.length, 1);
  const firstDestination = join(packDir, ...committedRows[0].resource.split("/"));
  assert.equal(existsSync(firstDestination), true);

  const second = await __promoteAssetToGameForTest(fx.root, {
    projectId: fx.projectId,
    elementId: fx.target.id,
    metadata: { ...METADATA, asset_id: "demo__hero_portrait_after_cleanup_crash__cc0-1-0" },
  }, dependencies(fx));
  const finalRows = readFileSync(second.manifestPath, "utf8").trim().split(/\r?\n/).map(JSON.parse);
  assert.equal(finalRows.length, 2);
  assert.equal(existsSync(firstDestination), true);
});

test("recovery rejects a replaced files junction before deleting an orphan", async (t) => {
  const fx = fixture(t);
  assert.equal(await crashPromotion(fx, "crash-after-destination"), 91);
  const packDir = join(fx.root, "games", "demo-game", "assets", "packs", "canvas-promotions");
  const filesRoot = join(packDir, "files");
  const outside = mkdtempSync(join(tmpdir(), "canvas-promotion-recovery-outside-"));
  const sentinel = join(outside, "sentinel.txt");
  writeFileSync(sentinel, "keep");
  t.after(() => rmSync(outside, { recursive: true, force: true }));
  rmSync(filesRoot, { recursive: true, force: true });
  symlinkSync(outside, filesRoot, process.platform === "win32" ? "junction" : "dir");
  await assert.rejects(
    () => __promoteAssetToGameForTest(fx.root, {
      projectId: fx.projectId,
      elementId: fx.target.id,
      metadata: METADATA,
    }, dependencies(fx)),
    /transaction files directory must be a physical directory/,
  );
  assert.equal(readFileSync(sentinel, "utf8"), "keep");
});

test("exclusive transaction temp refuses pre-planted paths and source extension is allowlisted", async (t) => {
  const fx = fixture(t);
  const packDir = join(fx.root, "games", "demo-game", "assets", "packs", "canvas-promotions");
  mkdirSync(packDir, { recursive: true });
  const markerTemp = join(packDir, ".asset-promotion-transaction.prepared.json.tmp");
  const outside = join(fx.root, "marker-temp-sentinel.txt");
  writeFileSync(outside, "do not overwrite");
  if (process.platform === "win32") writeFileSync(markerTemp, "stale temp");
  else symlinkSync(outside, markerTemp, "file");
  await __promoteAssetToGameForTest(fx.root, {
    projectId: fx.projectId,
    elementId: fx.target.id,
    metadata: METADATA,
  }, dependencies(fx));
  assert.equal(readFileSync(outside, "utf8"), "do not overwrite");

  const unsafe = fixture(t);
  const current = getProject(unsafe.root, unsafe.projectId);
  const target = current.elements.find((element) => element.id === unsafe.target.id);
  const unsafeSrc = target.src.replace(/\.[^.]+$/, ".txt");
  copyFileSync(join(process.env.CANVAS_PROJECTS_ROOT, unsafe.projectId, target.src), join(process.env.CANVAS_PROJECTS_ROOT, unsafe.projectId, unsafeSrc));
  updateProject(unsafe.root, unsafe.projectId, {
    elements: current.elements.map((element) => element.id === target.id ? {
      ...element,
      src: unsafeSrc,
      meta: {
        ...element.meta,
        technical_gate: { ...element.meta.technical_gate, source_ref: unsafeSrc },
        style_verdict: { ...element.meta.style_verdict, source_ref: unsafeSrc },
        style_decision: { ...element.meta.style_decision, source_ref: unsafeSrc },
      },
    } : element),
  });
  await assert.rejects(
    () => __promoteAssetToGameForTest(unsafe.root, {
      projectId: unsafe.projectId,
      elementId: unsafe.target.id,
      metadata: METADATA,
    }, dependencies(unsafe)),
    /source extension must be a Canvas/,
  );
});
