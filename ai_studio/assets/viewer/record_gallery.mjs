#!/usr/bin/env node
// Record a real Chrome session of the asset gallery via the DevTools Protocol
// screencast - one continuous take (grid -> open a pack -> open a model that
// rotates in 3D). Headed Chrome so WebGL/model-viewer renders on the GPU.
// Frames are encoded to mp4 with ffmpeg using their real timestamps (faithful
// timing, no compositing).
//
//   node ai_studio/assets/viewer/record_gallery.mjs --url http://localhost:8910 --out tmp/gallery.mp4
import { spawn, spawnSync } from "node:child_process";
import { readFileSync, writeFileSync, mkdirSync, rmSync, existsSync, readdirSync } from "node:fs";
import { join, resolve } from "node:path";

function parseArgs(argv) {
  const a = { url: "http://localhost:8910", out: "tmp/gallery.mp4", port: 9222, gallery: "tmp/lib-gallery",
    chrome: "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe", pack: "", asset: "", w: 1280, h: 800 };
  for (let i = 0; i < argv.length; i += 1) {
    const k = argv[i];
    if (k === "--url") a.url = argv[++i];
    else if (k === "--out") a.out = argv[++i];
    else if (k === "--port") a.port = Number(argv[++i]);
    else if (k === "--gallery") a.gallery = argv[++i];
    else if (k === "--chrome") a.chrome = argv[++i];
    else if (k === "--pack") a.pack = argv[++i];
    else if (k === "--asset") a.asset = argv[++i];
  }
  return a;
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// Pick a hero pack (most models, has a cover) + a member model with a glb.
function pickHero(galleryDir, wantPack, wantAsset) {
  const html = readFileSync(join(galleryDir, "index.html"), "utf8");
  const m = html.match(/window\.__VIEWER__\s*=\s*(\{[\s\S]*?\});<\/script>/) || html.match(/window\.__VIEWER__\s*=\s*(\{[\s\S]*?\})\s*<\/script>/);
  if (!m) throw new Error("could not find __VIEWER__ payload");
  const data = JSON.parse(m[1]);
  const assets = data.assets || [];
  const packs = data.packs || [];
  let pack = wantPack ? packs.find((p) => p.pack === wantPack) : null;
  if (!pack) {
    const scored = packs.filter((p) => p.coverImg && (p.count || 0) >= 8)
      .sort((x, y) => (y.count || 0) - (x.count || 0));
    // prefer a colourful/recognisable theme near the top
    pack = scored.find((p) => /food|furniture|space|nature|city|kitchen/i.test(p.title || p.pack)) || scored[0] || packs[0];
  }
  let asset = wantAsset ? assets.find((c) => c.id === wantAsset) : null;
  if (!asset) asset = assets.find((c) => c.pack === pack.pack && c.model) || assets.find((c) => c.model);
  return { pack, asset };
}

async function cdp() {
  // resolve the page websocket target
  let targets;
  for (let t = 0; t < 40; t += 1) {
    try { targets = await (await fetch(`http://127.0.0.1:${PORT}/json`)).json(); break; } catch { await sleep(250); }
  }
  if (!targets) throw new Error("chrome devtools not reachable");
  const page = targets.find((x) => x.type === "page") || targets[0];
  const ws = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((res, rej) => { ws.onopen = res; ws.onerror = rej; });
  let id = 0; const pending = new Map(); const handlers = [];
  ws.onmessage = (e) => {
    const msg = JSON.parse(e.data);
    if (msg.id && pending.has(msg.id)) { pending.get(msg.id)(msg.result); pending.delete(msg.id); }
    else if (msg.method) for (const h of handlers) h(msg);
  };
  const send = (method, params = {}) => new Promise((res) => { id += 1; pending.set(id, res); ws.send(JSON.stringify({ id, method, params })); });
  return { send, on: (fn) => handlers.push(fn), close: () => ws.close() };
}

const A = parseArgs(process.argv.slice(2));
const PORT = A.port;
const FRAMES = resolve("tmp/rec_frames");

async function main() {
  const { pack, asset } = pickHero(resolve(A.gallery), A.pack, A.asset);
  console.log("hero pack:", pack.pack, "| hero model:", asset ? asset.id : "(none)");

  rmSync(FRAMES, { recursive: true, force: true });
  mkdirSync(FRAMES, { recursive: true });

  const userDir = resolve("tmp/chrome_rec_profile");
  rmSync(userDir, { recursive: true, force: true });
  const chrome = spawn(A.chrome, [
    `--remote-debugging-port=${PORT}`, `--user-data-dir=${userDir}`,
    "--no-first-run", "--no-default-browser-check", "--new-window",
    `--window-size=${A.w},${A.h}`, "--window-position=40,40", "--hide-scrollbars",
    "--autoplay-policy=no-user-gesture-required",
    `${A.url}/#/packs`,
  ], { stdio: "ignore" });

  await sleep(1500);
  const c = await cdp();
  await c.send("Page.enable");
  await c.send("Runtime.enable");

  const frames = []; // {file, ts}
  let n = 0;
  c.on(async (msg) => {
    if (msg.method === "Page.screencastFrame") {
      const { data, sessionId, metadata } = msg.params;
      const file = join(FRAMES, `f_${String(++n).padStart(5, "0")}.jpg`);
      writeFileSync(file, Buffer.from(data, "base64"));
      frames.push({ file, ts: metadata.timestamp || (Date.now() / 1000) });
      c.send("Page.screencastFrameAck", { sessionId });
    }
  });

  const evalJs = (expr) => c.send("Runtime.evaluate", { expression: expr });
  // give the grid a moment to lay out, then start the continuous capture
  await sleep(1500);
  await c.send("Page.startScreencast", { format: "jpeg", quality: 75, maxWidth: A.w, maxHeight: A.h, everyNthFrame: 1 });

  // 1) packs grid - a gentle scroll for life
  await sleep(2200);
  await evalJs("window.scrollTo({top: 600, behavior:'smooth'})"); await sleep(1500);
  await evalJs("window.scrollTo({top: 0, behavior:'smooth'})"); await sleep(1200);

  // 2) open a pack
  await evalJs(`location.hash = '#/pack/${pack.pack}'`); await sleep(2600);
  await evalJs("window.scrollTo({top: 500, behavior:'smooth'})"); await sleep(1400);
  await evalJs("window.scrollTo({top: 0, behavior:'smooth'})"); await sleep(900);

  // 3) open a model - modal auto-rotates; nudge a manual spin too
  if (asset) {
    await evalJs(`location.hash = '#/asset/${asset.id}'`);
    await sleep(2500); // model load
    // manual drag-spin on the model-viewer for extra life
    const cx = Math.round(A.w / 2), cy = Math.round(A.h / 2);
    await c.send("Input.dispatchMouseEvent", { type: "mousePressed", x: cx, y: cy, button: "left", clickCount: 1 });
    for (let dx = 0; dx <= 320; dx += 16) { await c.send("Input.dispatchMouseEvent", { type: "mouseMoved", x: cx + dx, y: cy, button: "left" }); await sleep(35); }
    await c.send("Input.dispatchMouseEvent", { type: "mouseReleased", x: cx + 320, y: cy, button: "left", clickCount: 1 });
    await sleep(3500); // let auto-rotate carry it
  }

  await c.send("Page.stopScreencast");
  await sleep(300);
  c.close();
  // Kill ONLY the chrome instance we spawned (its own PID tree) - never the
  // user's Chrome, which is showing the gallery.
  try { spawnSync("taskkill", ["/PID", String(chrome.pid), "/T", "/F"], { stdio: "ignore" }); } catch { /* ignore */ }

  console.log("captured frames:", frames.length);
  if (frames.length < 5) throw new Error("too few frames captured");

  // build an ffmpeg concat with real per-frame durations (faithful timing)
  const t0 = frames[0].ts;
  let concat = "";
  for (let i = 0; i < frames.length; i += 1) {
    const dur = (i < frames.length - 1) ? Math.max(0.016, frames[i + 1].ts - frames[i].ts) : 0.2;
    concat += `file '${frames[i].file.replace(/\\/g, "/")}'\nduration ${dur.toFixed(3)}\n`;
  }
  concat += `file '${frames[frames.length - 1].file.replace(/\\/g, "/")}'\n`;
  const listFile = join(FRAMES, "concat.txt");
  writeFileSync(listFile, concat, "utf8");

  const out = resolve(A.out);
  mkdirSync(resolve(A.out, ".."), { recursive: true });
  const ff = spawnSync("ffmpeg", ["-y", "-f", "concat", "-safe", "0", "-i", listFile,
    "-vf", "scale=trunc(iw/2)*2:trunc(ih/2)*2,fps=30", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-movflags", "+faststart", out],
    { stdio: ["ignore", "ignore", "inherit"] });
  if (ff.status !== 0) throw new Error("ffmpeg failed");
  console.log("wrote", out, "(", (frames[frames.length - 1].ts - t0).toFixed(1), "s )");
}

main().catch((e) => { console.error("FATAL:", e.message); try { spawnSync("taskkill", ["/IM", "chrome.exe", "/F", "/T"], { stdio: "ignore" }); } catch {} process.exit(1); });
