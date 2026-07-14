// Cleanup tool dialog (T0207 UX redesign; lead pick 2026-07-04: «почему мы не делаем
// модальным окном? … в таком формате ок»): the Photoshop filter-dialog pattern, adapted so
// the live preview stays visible — a FLOATING, NON-modal palette over the stage with no
// backdrop (a web-modal backdrop would cover the canvas, and the preview paints ON the
// canvas at real zoom, which beats an in-dialog thumbnail for sprite work). ONE dialog at a
// time — the industry "only one uncommitted preview alive" rule (competitor audit
// contracts/alpha-and-cleanup.md) enforced structurally, exactly what a
// Photoshop modal does but without hiding the art; the inspector's launcher buttons are
// disabled while a dialog is open (inspector.js renderCleanup).
//
// Everything here is VIEW-STATE over the same preview/apply ops the CLI drives (tool
// parity): preview writes nothing anywhere; Apply = ONE journal entry (Ctrl+Z reverts
// byte-exact); Cancel/×/Esc = zero trace. The amber "preview — Apply to keep" chip on the
// element (workspace.js) stays the on-canvas truth signal.
import { el, fileUrl, setStatus } from "./app.js";
import { cleanupApplyAction, cleanupPreviewAction } from "./actions.js";
import {
  clearCleanupPreview,
  getCleanupPreview,
  loadCleanupBitmap,
  setCleanupPreview,
  setCleanupPreviewCompare,
} from "./workspace.js";

// ---- current-palette count (quantize) -------------------------------------------
// The element's CURRENT unique-color count (lead: «я бы хотел видеть сколько сейчас
// цветов»), computed client-side from the source bitmap with the SAME definition as
// quantize.py's _count_unique_colors (unique RGB triples over ALL pixels, alpha ignored,
// capped) so this number and the preview report's palette_size_before can never disagree.
// Cached by content-addressed src — the src IS the pixels, so the cache cannot go stale.
const paletteCountCache = new Map(); // element.src -> Promise<number>
const PALETTE_COUNT_CAP = 100000; // mirrors quantize.py UNIQUE_COLOR_CAP

function countElementColors(element) {
  const key = element.src;
  if (!paletteCountCache.has(key)) {
    paletteCountCache.set(key, new Promise((resolveCount, rejectCount) => {
      const img = new Image();
      img.onload = () => {
        try {
          const cv = document.createElement("canvas");
          cv.width = img.naturalWidth;
          cv.height = img.naturalHeight;
          const c2 = cv.getContext("2d", { willReadFrequently: true });
          c2.drawImage(img, 0, 0);
          const data = c2.getImageData(0, 0, cv.width, cv.height).data;
          const seen = new Set();
          for (let i = 0; i < data.length; i += 4) {
            seen.add((data[i] << 16) | (data[i + 1] << 8) | data[i + 2]);
            if (seen.size >= PALETTE_COUNT_CAP) break;
          }
          resolveCount(seen.size);
        } catch (error) {
          rejectCount(error);
        }
      };
      img.onerror = () => rejectCount(new Error(`could not load ${element.src} to count colors`));
      img.src = fileUrl(element);
    }));
  }
  return paletteCountCache.get(key);
}

// ---- dialog lifecycle -------------------------------------------------------------
let active = null; // { tool, elementId, root, keydown, debounce, onClose }

export function activeCleanupDialogTool() {
  return active ? active.tool : null;
}

// Close from anywhere with CANCEL semantics: the un-applied preview is dropped (zero
// trace). Apply clears the preview itself before calling this, so the double clear is a
// no-op there.
export function closeCleanupDialog() {
  if (!active) return;
  const { root, keydown, debounce, onClose } = active;
  active = null;
  clearTimeout(debounce.timer);
  document.removeEventListener("keydown", keydown, true);
  root.remove();
  if (getCleanupPreview()) clearCleanupPreview();
  if (onClose) onClose();
}

// Selection moved / element gone: inspector calls this on every render pass with the
// current single-image owner id (or null) — a dialog must never outlive its element.
export function syncCleanupDialog(ownerElementId) {
  if (active && active.elementId !== ownerElementId) closeCleanupDialog();
}

// Zero-change previews looked like "the tool does nothing" (lead: «квантизация как будто
// ничего не меняет») — when nothing changed, SAY it and say why/what to try.
function reportLineText(tool, report) {
  if (!report) return "";
  const changed = Number(report.changed_pixel_pct) || 0;
  if (tool === "denoise") {
    if (changed === 0) return `strength ${report.strength}: 0% changed — art is already clean at this strength`;
    return `strength ${report.strength}, ${report.changed_pixel_pct}% pixels changed`;
  }
  if (changed === 0) {
    return `0% changed — art already has ${report.palette_size_before} colors; lower the count to see an effect`;
  }
  return `palette ${report.palette_size_before} -> ${report.palette_size_after}, ${report.changed_pixel_pct}% pixels changed`;
}

export function openCleanupDialog(tool, element, { onClose } = {}) {
  if (active) return; // launchers are disabled while one is open — belt and suspenders

  const root = document.createElement("div");
  root.className = "cleanup-dialog";

  const head = document.createElement("div");
  head.className = "cleanup-dialog-head";
  const title = document.createElement("span");
  title.className = "panel-title";
  title.textContent = tool === "denoise" ? "Denoise" : "Quantize";
  const closeX = document.createElement("button");
  closeX.type = "button";
  closeX.className = "cleanup-dialog-close";
  closeX.textContent = "×";
  closeX.addEventListener("click", () => closeCleanupDialog());
  head.append(title, closeX);
  root.appendChild(head);

  const body = document.createElement("div");
  body.className = "cleanup-dialog-body";
  root.appendChild(body);

  const report = document.createElement("div");
  report.className = "insp-cleanup-report";

  const spinner = document.createElement("span");
  spinner.className = "insp-cleanup-spinner hidden";

  // ---- footer: Hold-to-see-original / Cancel / Apply -----------------------------
  const foot = document.createElement("div");
  foot.className = "cleanup-dialog-foot";

  const compareBtn = document.createElement("button");
  compareBtn.type = "button";
  compareBtn.className = "insp-btn-small hidden";
  // Direction made explicit (lead read it backwards once): the canvas already shows the
  // NEW result; holding this shows the ORIGINAL underneath.
  compareBtn.textContent = "Hold to see original";
  const startCompare = (event) => {
    event.preventDefault();
    setCleanupPreviewCompare(true);
  };
  const stopCompare = () => setCleanupPreviewCompare(false);
  compareBtn.addEventListener("mousedown", startCompare);
  compareBtn.addEventListener("touchstart", startCompare);
  compareBtn.addEventListener("mouseup", stopCompare);
  compareBtn.addEventListener("mouseleave", stopCompare);
  compareBtn.addEventListener("touchend", stopCompare);

  const spacer = document.createElement("span");
  spacer.className = "spacer";

  const cancelBtn = document.createElement("button");
  cancelBtn.type = "button";
  cancelBtn.className = "insp-btn-small";
  cancelBtn.textContent = "Cancel";
  cancelBtn.addEventListener("click", () => closeCleanupDialog());

  const applyBtn = document.createElement("button");
  applyBtn.type = "button";
  applyBtn.className = "primary insp-btn";
  applyBtn.textContent = "Apply";
  applyBtn.disabled = true;

  foot.append(compareBtn, spacer, cancelBtn, applyBtn);
  root.appendChild(foot);

  // ---- shared preview plumbing ----------------------------------------------------
  const debounce = { timer: null };
  let requestSeq = 0; // guards a slow-resolving preview from clobbering a newer one
  let computing = false;

  function syncButtons() {
    const preview = getCleanupPreview();
    const owns = Boolean(preview && preview.elementId === element.id && preview.tool === tool);
    compareBtn.classList.toggle("hidden", !owns);
    applyBtn.disabled = !owns || computing;
  }

  function runPreview(params) {
    clearTimeout(debounce.timer);
    debounce.timer = setTimeout(async () => {
      const seq = (requestSeq += 1);
      computing = true;
      spinner.classList.remove("hidden");
      syncButtons();
      try {
        const result = await cleanupPreviewAction(element.id, tool, params);
        if (seq !== requestSeq || !root.isConnected) return; // superseded or dialog closed
        const bitmap = await loadCleanupBitmap(result.preview_base64);
        if (seq !== requestSeq || !root.isConnected) return;
        setCleanupPreview({ elementId: element.id, bitmap, tool: result.tool, params: result.params, report: result.report });
        report.textContent = reportLineText(tool, result.report);
      } catch (error) {
        setStatus(error.message, true);
        clearCleanupPreview();
        report.textContent = "";
      } finally {
        if (seq === requestSeq) {
          computing = false;
          spinner.classList.add("hidden");
        }
        syncButtons();
      }
    }, 350);
  }

  applyBtn.addEventListener("click", () => {
    const preview = getCleanupPreview();
    if (!preview || preview.elementId !== element.id || preview.tool !== tool) return;
    const { params } = preview;
    // Clear + close BEFORE the mutation lands (Photoshop's OK): the mutation's own
    // inspector rebuild must not see a preview that is about to stop existing.
    clearCleanupPreview();
    closeCleanupDialog();
    cleanupApplyAction(element.id, tool, params, applyBtn);
  });

  // ---- tool controls ----------------------------------------------------------------
  if (tool === "quantize") {
    // Current palette size + the slider SEEDED from it (lead: «ползунок от текущего
    // значения») — dragging left = fewer colors. Seeding only happens while the control
    // still shows the untouched default and nothing previews yet.
    const paletteLine = document.createElement("div");
    paletteLine.className = "insp-cleanup-report";
    paletteLine.textContent = "Current palette: counting…";
    body.appendChild(paletteLine);

    const row = document.createElement("div");
    row.className = "insp-cleanup-row";
    const range = document.createElement("input");
    range.type = "range";
    range.min = "2";
    range.max = "256";
    range.value = "32";
    range.className = "insp-range";
    const num = document.createElement("input");
    num.type = "number";
    num.min = "2";
    num.max = "256";
    num.value = "32";
    num.className = "insp-input insp-cleanup-num";
    row.append(range, num, spinner);
    body.appendChild(row);

    const ditherRow = document.createElement("label");
    ditherRow.className = "insp-check";
    const ditherCheck = document.createElement("input");
    ditherCheck.type = "checkbox";
    const ditherLabel = document.createElement("span");
    ditherLabel.textContent = "Dither";
    ditherRow.append(ditherCheck, ditherLabel);
    body.appendChild(ditherRow);

    body.appendChild(report);

    countElementColors(element)
      .then((count) => {
        if (!paletteLine.isConnected) return; // dialog closed meanwhile
        paletteLine.textContent = count >= PALETTE_COUNT_CAP
          ? `Current palette: ${PALETTE_COUNT_CAP.toLocaleString()}+ colors`
          : `Current palette: ${count} colors`;
        if (num.value === "32" && !getCleanupPreview()) {
          const seeded = Math.min(256, Math.max(2, count));
          num.value = String(seeded);
          range.value = String(seeded);
        }
      })
      .catch(() => {
        if (paletteLine.isConnected) paletteLine.textContent = "Current palette: unavailable";
      });

    const clampColors = (raw) => {
      const n = Math.round(Number(raw));
      return Number.isFinite(n) ? Math.min(256, Math.max(2, n)) : null;
    };
    const params = () => ({ colors: clampColors(num.value) ?? 32, dither: ditherCheck.checked });

    // The range thumb is always a valid clamped int — sync the number field and preview
    // immediately.
    range.addEventListener("input", () => {
      num.value = range.value;
      runPreview(params());
    });
    // Typing must NEVER rewrite the field mid-edit (clamping a mid-typed "1" of "128" back
    // to "2" would fight every keystroke) — the thumb tracks along on each valid keystroke;
    // the field's text normalizes on commit (change/blur), the panel-wide convention.
    num.addEventListener("input", () => {
      const clamped = clampColors(num.value);
      if (clamped == null) return; // mid-edit / empty — nothing valid to preview yet
      range.value = String(clamped);
      runPreview({ colors: clamped, dither: ditherCheck.checked });
    });
    num.addEventListener("change", () => {
      const clamped = clampColors(num.value) ?? 32;
      num.value = String(clamped);
      range.value = String(clamped);
      runPreview({ colors: clamped, dither: ditherCheck.checked });
    });
    ditherCheck.addEventListener("change", () => runPreview(params()));
  } else {
    // Denoise. 0 = off (lead: «нужна какая-то 0 сила?») — a real OFF state, the default;
    // clicking 0 drops the live preview. 1..3 preview (median filter: strength = how
    // aggressively isolated speckle pixels are absorbed; edges survive, unlike a blur).
    const row = document.createElement("div");
    row.className = "insp-cleanup-row";
    const seg = document.createElement("div");
    seg.className = "insp-segmented";
    let strength = 0;
    const buttons = [0, 1, 2, 3].map((value) => {
      const btn = document.createElement("button");
      btn.type = "button";
      btn.className = value === strength ? "insp-seg-btn primary" : "insp-seg-btn";
      btn.textContent = String(value);
      seg.appendChild(btn);
      return btn;
    });
    const hint = document.createElement("span");
    hint.className = "insp-cleanup-hint";
    hint.textContent = "0 off · 1 light · 3 strong";
    row.append(seg, hint, spinner);
    body.appendChild(row);
    body.appendChild(report);

    const setHighlight = (value) => {
      strength = value;
      for (const other of buttons) other.classList.toggle("primary", Number(other.textContent) === value);
    };
    for (const btn of buttons) {
      btn.addEventListener("click", () => {
        const value = Number(btn.textContent);
        if (value === strength) return;
        setHighlight(value);
        if (value === 0) {
          clearTimeout(debounce.timer);
          clearCleanupPreview();
          report.textContent = "";
          syncButtons();
          return;
        }
        runPreview({ strength: value });
      });
    }
  }

  // Esc = Cancel, captured so the canvas's own Esc chain (selection/region-edit) does not
  // also fire — same convention as the prompt modal.
  const keydown = (event) => {
    if (event.key !== "Escape") return;
    event.stopPropagation();
    event.preventDefault();
    closeCleanupDialog();
  };
  document.addEventListener("keydown", keydown, true);

  el("stage").appendChild(root);
  active = { tool, elementId: element.id, root, keydown, debounce, onClose };
  syncButtons();
}
