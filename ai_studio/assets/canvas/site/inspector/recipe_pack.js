import { state } from "../app.js";
import {
  expandRecipePromptAction,
  generateFromRecipeAction,
  packPreviewAction,
  packSliceAction,
  patchRecipeAction,
} from "../actions.js";
import { estimatePackSheetCount, normalizeSmartQuotes, PACK_AXES_SKELETON, parseAxesJson } from "./contracts.js";
import { field, numberInput, selectInput, textareaInput } from "./controls.js";
import { collapsible, openPromptModal, readOnly, smallBtn } from "./primitives.js";
// ---- pack mode (T0332 v2: contracts/recipe-pack.md) --------------------------------------
//
// Smart quotes (typed by a phone keyboard / autocorrect / pasted from a doc) are the single
// most common way a hand-typed axes JSON silently fails to parse — JSON.parse has no notion
// of «»/""/'' curly quotes. Straightened to ASCII BEFORE parsing (build-spec: "нормализация
// «умных кавычек» перед parse"). Pure/testable in isolation from the DOM textarea that calls
// it (tests/pack_ui.test.mjs).
// A skeleton EXAMPLE prefilled into an empty axes textarea (build-spec: "префилл валидным
// скелетом-примером") — a starting point to edit, never auto-committed on its own; the lead's
// own blur still has to accept (or edit) it before patchRecipeAction ever sends anything.
// JSON.parse with a LINE/COLUMN pointer on failure (build-spec: "ошибка парсера с ПОЗИЦИЕЙ
// (строка/столбец)"). Modern V8 often ALREADY names a "(line X column Y)" pair right in the
// SyntaxError message — reused verbatim when present (most accurate); when it names only a
// 0-based character offset ("at position N") instead, this walks the text once to translate
// that into a line/column; some V8 error shapes (e.g. a trailing comma inside an array) give
// neither, and the raw message is shown as-is rather than a fabricated position. Throws a
// plain Error (never the raw SyntaxError) so every caller shows ONE consistent message shape.
// Also checks the coarse SHAPE (a plain object, not an array/primitive) — the per-axis
// semantic rules (non-empty string arrays, etc.) stay server-side (ops.normalizeRecipePack is
// the authority; a bad shape there still 400s, surfaced by patchRecipeAction's error toast).
// A client-side ESTIMATE of the pack's sheet count (mirrors expand_jobs.py's own
// `itertools.product` over every axis EXCEPT `vary` — the "big" axes) — purely for the
// Generate/Preview busy-label/title text, computed before the real expander ever runs; the
// REAL count (and any axes/vary validation error) comes from Preview pack / Generate
// themselves. `vary` not matching any axis key is not an error here (that is the expander's
// own loud refusal at preview/generate time) — it just means no axis is excluded.
// Fresh pack-mode default: a WORKING config, not an empty draft (lead walked a chain of
// incomplete-config errors on 2026-07-07: empty vary -> pick vary -> other axis lacks a
// {slot}). ONE axis with vary preselected has no big axes at all, so the prompt needs no
// {slot} and Preview passes right after the toggle; errors only appear once the lead ADDS
// axes — at which point the Vary hint below has already stated the slot rule.
const DEFAULT_PACK_TEMPLATE = () => ({
  axes: { grade: ["rusty", "plain", "gilded"] },
  vary: "grade",
  grid: [3, 3],
  max_jobs: 12,
});

// `pack` REPLACES wholesale on every patch (ops.mjs's own doc: "patch ЗАМЕНЯЕТ pack
// целиком") — every pack-field commit below sends the FULL `{...recipe.pack, ...fieldPatch}`
// object, never a bare `{vary: next}`, mirroring cli.mjs's own recipe-set read-modify-write.
function commitPackPatch(group, recipe, fieldPatch) {
  patchRecipeAction(group.id, { pack: { ...recipe.pack, ...fieldPatch } });
}

const PACK_BG_KEYS = new Set(["#ff00ff", "#00ff00"]); // mirrors ops.mjs's BG_KEY_BACKGROUND keys

// Axes JSON textarea: blur-only validation (build-spec: "валидация на blur"), a skeleton
// prefill when axes is empty, smart-quote normalization, and a line/column-pointing error
// shown INLINE (never just a toast) so the lead can see exactly where the JSON broke.
function renderPackAxesField(group, recipe, body) {
  const textarea = document.createElement("textarea");
  textarea.className = "insp-input insp-pack-axes";
  textarea.rows = 6;
  const hasAxes = recipe.pack.axes && Object.keys(recipe.pack.axes).length;
  textarea.value = hasAxes ? JSON.stringify(recipe.pack.axes, null, 2) : PACK_AXES_SKELETON;
  body.appendChild(field("Axes (JSON)", textarea));

  const errorEl = document.createElement("div");
  errorEl.className = "insp-pack-error";
  errorEl.style.display = "none";
  body.appendChild(errorEl);

  textarea.addEventListener("blur", () => {
    const normalized = normalizeSmartQuotes(textarea.value);
    if (normalized !== textarea.value) textarea.value = normalized;
    let parsed;
    try {
      parsed = parseAxesJson(normalized);
    } catch (error) {
      errorEl.textContent = error.message;
      errorEl.style.display = "";
      return;
    }
    errorEl.style.display = "none";
    errorEl.textContent = "";
    if (JSON.stringify(parsed) === JSON.stringify(recipe.pack.axes || {})) return; // unchanged: no commit
    // Never leave vary dangling after an axes edit (lead's stumble 2026-07-07): if the
    // current vary is no longer a key of the new axes, re-point it at the first key in the
    // SAME commit — one journal entry, and Preview can't hit "vary '' is not a key".
    const patch = { axes: parsed };
    const keys = Object.keys(parsed);
    if (!keys.includes(recipe.pack.vary)) patch.vary = keys[0] || "";
    commitPackPatch(group, recipe, patch);
  });
}

// Vary select: options come from the axes keys (build-spec: "vary select (from axes keys)")
// — the CURRENT value is always included even if it is not (yet) an axes key, so the select
// never silently jumps to a different displayed value out from under an in-progress edit.
function renderPackVaryField(group, recipe, body) {
  const options = Object.keys(recipe.pack.axes || {});
  if (recipe.pack.vary && !options.includes(recipe.pack.vary)) options.push(recipe.pack.vary);
  if (!options.length) options.push("");
  body.appendChild(field("Vary", selectInput(recipe.pack.vary || "", options, (next) => commitPackPatch(group, recipe, { vary: next }))));
  // The one rule the error chain was teaching one bounce at a time (2026-07-07) — state it
  // upfront instead: vary spreads across the sheet cells, every OTHER axis must be a prompt
  // slot (the expander refuses an axis that affects nothing).
  const varyHint = document.createElement("div");
  varyHint.className = "insp-region-hint";
  varyHint.textContent = "Cells vary by this axis. Every OTHER axis must appear in the prompt as {axis} — one sheet per combination.";
  body.appendChild(varyHint);
  // Third bounce of the same stumble (lead hit the expander's missing-slot refusal from the
  // Generate button itself, 2026-07-07): pre-check the slot rule CLIENT-side and warn before
  // any button is pressed. Advisory only — the expander stays the real gate.
  const bigAxes = Object.keys(recipe.pack.axes || {}).filter((axis) => axis !== recipe.pack.vary);
  const missingSlots = bigAxes.filter((axis) => !(recipe.prompt || "").includes(`{${axis}}`));
  if (missingSlots.length) {
    const slotWarn = document.createElement("div");
    slotWarn.className = "insp-pack-error";
    slotWarn.textContent = `Prompt is missing ${missingSlots.map((axis) => `{${axis}}`).join(", ")} — Preview/Generate will refuse until every non-vary axis has its slot.`;
    body.appendChild(slotWarn);
  }
}

// Grid select: 2x2 / 3x3 (build-spec: "grid select (2x2|3x3)") — a stored grid outside that
// pair (e.g. set via the CLI/an agent) is still shown, appended as its own option, same
// never-silently-jump rule as Vary above.
function renderPackGridField(group, recipe, body) {
  const options = ["2x2", "3x3"];
  const current = `${Number(recipe.pack.grid[0])}x${Number(recipe.pack.grid[1])}`;
  if (!options.includes(current)) options.push(current);
  const select = selectInput(current, options, (next) => {
    const match = /^(\d+)x(\d+)$/i.exec(next);
    if (!match) return;
    commitPackPatch(group, recipe, { grid: [Number(match[1]), Number(match[2])] });
  });
  body.appendChild(field("Grid", select));
}

function renderPackMaxJobsField(group, recipe, body) {
  body.appendChild(
    field(
      "Max jobs",
      numberInput(recipe.pack.max_jobs, (next) => commitPackPatch(group, recipe, { max_jobs: Math.max(1, Math.round(next)) })),
    ),
  );
}

// bg_key/n_candidates (T0332 v2: `params` unfrozen for exactly these two fields, plus
// size/quality — see ops.normalizeRecipePatch) live here, INSIDE the pack sub-block, since
// this phase adds no other params UI. bg_key's mode-aware hint + pair-validation is
// build-spec-mandated ("на blur, а не только на generate"): patch-time only checks generic
// hex format (any color is a legal single-image bg_key), so an off-pair value still commits
// here — the warning is ADVISORY, refusal itself stays where the build-spec puts it
// (packPreview/generateFromRecipe's pack branch). A successful blur-commit re-renders the
// whole inspector (applyMutation -> refresh), which recomputes this same warning from the
// freshly-stored value — no separate on-blur DOM patch needed.
function renderPackParamsFields(group, recipe, body) {
  // Defensive guard (recipe.params SHOULD always be an object per defaultRecipe, but a
  // hand-edited/legacy project.json could still omit it) — render sensible defaults instead
  // of throwing on a missing blob.
  const params = recipe.params || {};
  const bgKey = params.bg_key;
  const nCandidates = params.n_candidates != null ? params.n_candidates : 1;

  const bgKeyInput = document.createElement("input");
  bgKeyInput.type = "text";
  bgKeyInput.className = "insp-input";
  bgKeyInput.value = bgKey || "";
  bgKeyInput.addEventListener("blur", () => {
    const next = bgKeyInput.value.trim();
    if (!next || next === bgKey) return;
    patchRecipeAction(group.id, { params: { bg_key: next } });
  });
  body.appendChild(field("BG key", bgKeyInput));

  const bgKeyHint = document.createElement("div");
  bgKeyHint.className = "insp-region-hint";
  bgKeyHint.textContent = "Pack mode: only #ff00ff / #00ff00 — the key color gets baked into the sheet.";
  body.appendChild(bgKeyHint);

  if (!PACK_BG_KEYS.has(String(bgKey || "").toLowerCase())) {
    const warn = document.createElement("div");
    warn.className = "insp-pack-error";
    warn.textContent = `Current bg_key ${bgKey || "(none)"} is not #ff00ff/#00ff00 — Preview pack/Generate will refuse until this matches.`;
    body.appendChild(warn);
  }

  body.appendChild(
    field(
      "Candidates",
      numberInput(nCandidates, (next) => patchRecipeAction(group.id, { params: { n_candidates: Math.max(1, Math.round(next)) } })),
    ),
  );
}

// Recipe card surface (T0239 increment 1): additive, shown only when the selected group
// carries a `recipe` blob (same "presence of the additive field" pattern as
// renderGroupBackground/renderAlphaPlates — a plain group renders no Recipe section at
// all). Prompt + Engine are live-editable through patchRecipeAction (one journal entry
// per commit, mirrors every other inspector field). Generate runs the T0239-2 flow via
// generateFromRecipeAction (long-op queue, codex/agy = minutes; disabled on an empty
// prompt — the op would refuse loudly anyway, the disable just says WHY up front). The
// Style dropdown (T0239 increment 3) lists every style-card group of THIS project by name;
// picking one commits recipe.style_ref through the SAME patchRecipeAction the other fields
// use — style cards mix their prompt + ref image into the next Generate (ops.mjs).
export function renderRecipe(group, root) {
  const recipe = group.recipe;
  if (!recipe || typeof recipe !== "object") return;
  const body = collapsible(root, "recipe", "Recipe");

  const promptField = field("Prompt", textareaInput(recipe.prompt, (next) => patchRecipeAction(group.id, { prompt: next })));
  body.appendChild(promptField);

  // T0250 (lead: "промпт тяжело вот так читать" — the 3-row textarea reads a real prompt
  // badly). Opens the SAME prompt in a large centered modal; Save commits through the
  // identical patchRecipeAction the inline textarea uses (one journal entry either way).
  const editPromptBtn = smallBtn("Edit", () =>
    openPromptModal(group.name || "Prompt", recipe.prompt, (next) => patchRecipeAction(group.id, { prompt: next })),
  );
  editPromptBtn.classList.add("insp-prompt-edit-btn");
  editPromptBtn.title = "Open the prompt in a large editor";
  body.appendChild(editPromptBtn);

  const engineField = field(
    "Engine",
    selectInput(recipe.engine || "codex", ["codex", "gemini", "both"], (next) => patchRecipeAction(group.id, { engine: next })),
  );
  body.appendChild(engineField);
  // Pack mode runs on any engine (agy grid adherence smoke-checked 2026-07-07; "both" allowed
  // per lead decision 2026-07-07 — cost is the lead's call). The hint's one job is to make the
  // "both" price visible BEFORE the click: every sheet generates on both engines, 2x the calls.
  if (recipe.pack && (recipe.engine || "codex") === "both") {
    const engineHint = document.createElement("div");
    engineHint.className = "insp-region-hint";
    engineHint.textContent = "Engine 'both': every sheet generates on codex AND agy — 2× the paid calls.";
    body.appendChild(engineHint);
  }

  const styleSelect = document.createElement("select");
  styleSelect.className = "insp-input";
  const noneOption = document.createElement("option");
  noneOption.value = "";
  noneOption.textContent = "None";
  styleSelect.appendChild(noneOption);
  const styleCards = (state.project ? state.project.groups || [] : []).filter(
    (candidate) => candidate.style && typeof candidate.style === "object",
  );
  for (const styleCard of styleCards) {
    const option = document.createElement("option");
    option.value = styleCard.id;
    option.textContent = styleCard.name || styleCard.id;
    styleSelect.appendChild(option);
  }
  styleSelect.value = recipe.style_ref || "";
  styleSelect.addEventListener("change", () => patchRecipeAction(group.id, { style_ref: styleSelect.value || null }));
  body.appendChild(field("Style", styleSelect));

  // ---- Pack mode (T0332 v2 phase C) -----------------------------------------------
  //
  // "Слить": pack is a MODE of the recipe card, not a third card type — this toggle just
  // sets/clears `recipe.pack` (a full default template on / null off); every other pack
  // control below only renders once it is set. The Generate button further down is the SAME
  // button either way — it branches on recipe.pack server-side (ops.generateFromRecipe).
  const packToggleRow = document.createElement("label");
  packToggleRow.className = "insp-check";
  const packToggleCheck = document.createElement("input");
  packToggleCheck.type = "checkbox";
  packToggleCheck.checked = !!recipe.pack;
  packToggleCheck.addEventListener("change", () => {
    patchRecipeAction(group.id, { pack: packToggleCheck.checked ? DEFAULT_PACK_TEMPLATE() : null });
  });
  const packToggleLabel = document.createElement("span");
  packToggleLabel.textContent = "Pack mode";
  packToggleRow.append(packToggleCheck, packToggleLabel);
  body.appendChild(packToggleRow);

  if (recipe.pack) {
    renderPackAxesField(group, recipe, body);
    renderPackVaryField(group, recipe, body);
    renderPackGridField(group, recipe, body);
    renderPackMaxJobsField(group, recipe, body);
    renderPackParamsFields(group, recipe, body);
  }

  // UX finding: recipe.expanded non-empty AND pack set — pack mode ignores it entirely (it
  // always sends recipe.prompt VERBATIM, never resolveRecipePromptText/expanded), so without
  // this the disappearing Expand-prompt button + a silently-ignored Expanded block would read
  // as a bug, not a mode.
  if (recipe.pack && recipe.expanded) {
    const banner = document.createElement("div");
    banner.className = "insp-pack-banner";
    banner.textContent = "Pack generates from the base prompt — expanded is not used. Move any needed detail into the prompt or the style card.";
    body.appendChild(banner);
  }

  // "last pack: <ts>, N sheets, M failed" (build-spec UX finding) — only for a PACK-shaped
  // last_run ({at, verdict, run_group_id, failed}); the single-image branch's own last_run
  // shape ({at, result_element_id, verdict}) has no `failed` array, so this line never shows
  // for a plain single-image run. N sheets is derived from the run group's OWN sheet elements
  // (nothing else persists a "sheets in this run" count) — a sheet is any element in
  // last_run.run_group_id carrying this card's meta.pack.cells (the full-manifest marker).
  if (recipe.last_run && Array.isArray(recipe.last_run.failed)) {
    const runElements = recipe.last_run.run_group_id
      ? (state.project ? state.project.elements || [] : []).filter(
          (el) => el.groupId === recipe.last_run.run_group_id && el.meta && el.meta.pack && Array.isArray(el.meta.pack.cells) && el.meta.pack.cardId === group.id,
        )
      : [];
    const ts = recipe.last_run.at ? new Date(recipe.last_run.at).toLocaleString() : "—";
    body.appendChild(readOnly("Last pack", `${ts}, ${runElements.length} sheet(s), ${recipe.last_run.failed.length} failed`));
  }

  if (recipe.pack) {
    const previewBtn = smallBtn("Preview pack", () => packPreviewAction(group.id, previewBtn));
    previewBtn.title = "Ephemeral: shows sheet count + per-sheet prompts — the ONLY honest per-cell preview (single Generate assembles a different prompt)";
    body.appendChild(previewBtn);

    if (state.packPreview && state.packPreview.cardId === group.id) {
      const preview = state.packPreview;
      const summary = document.createElement("div");
      summary.className = "insp-align-caption";
      summary.textContent = `Preview: ${preview.sheets} sheet(s)${preview.style_ref_image ? " · style ref image included" : ""}`;
      body.appendChild(summary);
      // Per-sheet prompts as inline SPOILERS, not View-buttons + modal (lead's ask,
      // 2026-07-07): native <details> — expands in place, no collapsed-state bookkeeping.
      for (const job of preview.jobs || []) {
        const spoiler = document.createElement("details");
        spoiler.className = "insp-pack-preview-sheet";
        const summaryEl = document.createElement("summary");
        summaryEl.textContent = job.name;
        const promptPre = document.createElement("pre");
        promptPre.className = "insp-pack-preview-prompt";
        promptPre.textContent = job.prompt;
        spoiler.append(summaryEl, promptPre);
        body.appendChild(spoiler);
      }
    }
  }

  const generateBtn = document.createElement("button");
  generateBtn.type = "button";
  generateBtn.className = "primary insp-btn";
  generateBtn.textContent = "Generate";
  const emptyPrompt = !String(recipe.prompt || "").trim();
  generateBtn.disabled = emptyPrompt;
  const packSheetEstimate = recipe.pack ? estimatePackSheetCount(recipe.pack) : 0;
  const packEngineLabel = recipe.engine === "gemini" ? "agy" : recipe.engine === "both" ? "codex+agy, ×2 calls" : "codex";
  const packBusyLabel = `Generating pack… (~${packSheetEstimate} sheet(s), ${packEngineLabel}, ~30-60s each)`; // …
  generateBtn.title = emptyPrompt
    ? "Write a prompt first"
    : recipe.pack
      ? `Generate the pack (~${packSheetEstimate} sheet(s) — see Preview pack for the exact count/prompts)`
      : "Generate (codex/agy — takes minutes)";
  generateBtn.addEventListener("click", () =>
    generateFromRecipeAction(group.id, generateBtn, recipe.pack ? { busyLabel: packBusyLabel } : undefined),
  );
  body.appendChild(generateBtn);

  if (recipe.pack) {
    const canSlice = !!(recipe.last_run && recipe.last_run.run_group_id);
    const sliceBtn = document.createElement("button");
    sliceBtn.type = "button";
    sliceBtn.className = "insp-btn";
    sliceBtn.textContent = "Slice pack";
    sliceBtn.disabled = !canSlice;
    sliceBtn.title = canSlice ? "Detect + slice every sheet of the last pack run" : "Generate a pack run first";
    sliceBtn.addEventListener("click", () => packSliceAction(group.id, sliceBtn));
    body.appendChild(sliceBtn);
  }

  // ---- Expand-prompt (T0239 increment 4) ------------------------------------------
  // Hidden in pack mode (UX finding): pack always sends recipe.prompt verbatim, so Expand
  // has nothing to feed — showing it would invite generating a text nothing ever reads.
  if (!recipe.pack) {
    const expandBtn = document.createElement("button");
    expandBtn.type = "button";
    expandBtn.className = "insp-btn";
    expandBtn.textContent = "Expand prompt";
    expandBtn.disabled = emptyPrompt;
    expandBtn.title = emptyPrompt ? "Write a prompt first" : "Expand into a labeled generation-prompt template (codex, ~1 min)";
    expandBtn.addEventListener("click", () => expandRecipePromptAction(group.id, expandBtn));
    body.appendChild(expandBtn);
  }

  // The Expanded block only renders once recipe.expanded exists: an editable textarea + a
  // large-editor Edit modal (both commit through patchRecipeAction({expanded}), same
  // pattern as the Prompt field above) + the "Send expanded" checkbox (defaults true,
  // patchRecipeAction({use_expanded})) + Discard (patches {expanded: null} — "remove the
  // stale expansion", one journal entry). The muted hint states which text Generate will
  // ACTUALLY send right now, mirroring resolveRecipePromptText's own rule exactly
  // (`use_expanded && expanded ? expanded : prompt`) — EXCEPT in pack mode, where it never
  // reads expanded/use_expanded at all (T0332 v2), so the hint says that instead.
  if (recipe.expanded != null) {
    const expandedField = field(
      "Expanded",
      textareaInput(recipe.expanded, (next) => patchRecipeAction(group.id, { expanded: next })),
    );
    body.appendChild(expandedField);

    const editExpandedBtn = smallBtn("Edit", () =>
      openPromptModal(`${group.name || "Recipe"} — expanded`, recipe.expanded, (next) => patchRecipeAction(group.id, { expanded: next })),
    );
    editExpandedBtn.classList.add("insp-prompt-edit-btn");
    editExpandedBtn.title = "Open the expanded prompt in a large editor";
    body.appendChild(editExpandedBtn);

    const sendRow = document.createElement("label");
    sendRow.className = "insp-check";
    const sendCheck = document.createElement("input");
    sendCheck.type = "checkbox";
    sendCheck.checked = recipe.use_expanded !== false;
    sendCheck.addEventListener("change", () => patchRecipeAction(group.id, { use_expanded: sendCheck.checked }));
    const sendLabel = document.createElement("span");
    sendLabel.textContent = "Send expanded";
    sendRow.append(sendCheck, sendLabel);
    body.appendChild(sendRow);

    const discardBtn = smallBtn("Discard", () => patchRecipeAction(group.id, { expanded: null }));
    discardBtn.title = "Remove the expanded text — Generate falls back to the short prompt";
    body.appendChild(discardBtn);

    const willSendExpanded = !recipe.pack && recipe.use_expanded !== false && recipe.expanded;
    const hint = document.createElement("div");
    hint.className = "insp-region-hint";
    hint.textContent = recipe.pack
      ? "Pack mode ignores the expanded text — it always generates from the short prompt."
      : willSendExpanded
        ? "Generate sends the expanded text."
        : "Generate sends the short prompt (expanded text kept, not sent).";
    body.appendChild(hint);
  }
}
